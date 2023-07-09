import orjson
import os
import requests
import tempfile
from collections import defaultdict
from contextlib import contextmanager
from discord_webhook import DiscordEmbed, DiscordWebhook
from dotenv import load_dotenv
from flask import abort, Flask, render_template, request, send_file
from flask.typing import ResponseReturnValue
from flask_httpauth import HTTPBasicAuth
from flask_sqlalchemy import SQLAlchemy
from pathlib import Path
from sqlalchemy import Column, DateTime, Integer, String
from sqlalchemy.sql import func
from typing import Generator
from werkzeug.security import check_password_hash, generate_password_hash
from zipfile import ZipFile

load_dotenv()

data_directory = os.getenv("DATA_DIRECTORY", "")
data_directory = None if data_directory == "" else data_directory

app = Flask(__name__, instance_path=data_directory)
app.config["MAX_CONTENT_LENGTH"] = 64 * 1024 * 1024
app.config["SQLALCHEMY_DATABASE_URI"] = "sqlite:///database.sqlite"

auth = HTTPBasicAuth()
db = SQLAlchemy(app)

admin_username = os.environ["ADMIN_USERNAME"]
admin_password = generate_password_hash(os.environ["ADMIN_PASSWORD"])

submitter_username = os.environ["SUBMITTER_USERNAME"]
submitter_password = generate_password_hash(os.environ["SUBMITTER_PASSWORD"])

users = {
    admin_username: admin_password,
    submitter_username: submitter_password,
}

icfpc_token = os.environ["ICFPC_TOKEN"]
discord_webhook = os.environ["DISCORD_WEBHOOK"]

project_root = Path(__file__).parent
while not (project_root / "problems").is_dir():
    project_root = project_root.parent

problems_dir = project_root / "problems"

submissions_dir = Path(app.instance_path) / "submissions"
submissions_dir.mkdir(exist_ok=True)

class Submission(db.Model):
    id = Column(Integer, primary_key=True)
    problem_id = Column(Integer, nullable=False)
    score = Column(Integer, nullable=False)
    target = Column(String, nullable=False)
    created_at = Column(DateTime(timezone=True), nullable=False, server_default=func.now())

with app.app_context():
    db.create_all()

@auth.verify_password
def verify_password(username: str, password: str) -> str | None:
    if username in users and check_password_hash(users[username], password):
        return username

@auth.get_user_roles
def get_user_roles(username: str) -> list[str]:
    if username == admin_username:
        return ["admin", "submitter"]
    elif username == submitter_username:
        return ["submitter"]
    else:
        return []

@app.template_filter("format_small")
def format_small(value: int) -> str:
    return f"{value:.0f}"

@app.template_filter("format_large")
def format_large(value: int) -> str:
    return f"{value:,.0f}"

@contextmanager
def temporary_file() -> Generator[Path, None, None]:
    fd, name = tempfile.mkstemp()
    os.close(fd)

    path = Path(name)

    try:
        yield path
    finally:
        path.unlink()

@app.get("/")
@auth.login_required(role="admin")
def index() -> ResponseReturnValue:
    problem_files = [file for file in problems_dir.iterdir() if file.is_file() and file.name.endswith(".json")]

    problems_by_id = {}
    for file in sorted(problem_files, key=lambda file: int(file.stem)):
        problems_by_id[int(file.stem)] = orjson.loads(file.read_text(encoding="utf-8"))

    submissions = [row[0] for row in db.session.execute(db.select(Submission)).all()]
    submissions_by_problem = defaultdict(list)
    for submission in submissions:
        submissions_by_problem[submission.problem_id].append(submission)

    problems = []
    score = 0
    recent_submissions = sorted(submissions, key=lambda submission: submission.created_at, reverse=True)[:10]

    for id, problem in problems_by_id.items():
        submissions = sorted(submissions_by_problem[id], key=lambda submission: submission.score, reverse=True)

        if len(submissions) > 0:
            score += submissions[0].score

        for i, submission in enumerate(submissions):
            if i + 1 < len(submissions):
                submission.score_delta = submission.score - submissions[i + 1].score
            else:
                submission.score_delta = submission.score

        problems.append({
            "id": id,
            "problem": problem,
            "submissions": submissions,
        })

    return render_template("index.html", problems=problems, score=score, recent_submissions=recent_submissions)

@app.get("/download")
@auth.login_required(role="admin")
def download_all() -> ResponseReturnValue:
    submissions = [row[0] for row in db.session.execute(db.select(Submission)).all()]
    submissions_by_problem = defaultdict(list)
    for submission in submissions:
        submissions_by_problem[submission.problem_id].append(submission)

    readme_content = """
# camel_case submissions

This directory contains my best solutions for each problem, along with copies of the source code that were used to generate them. This directory is laid out as follows:
```
- problems/
    - <problem id>.json - the raw problem data for each problem
- <problem id>/
    - solution.json - my best solution for this problem
    - source.zip - the source code used to generate my best solution to this problem
- README.md - the file you're looking at right now
```

## Usage

You can run the program that generated my best solution for problem `<problem id>` as follows:
1. Extract `<problem id>/source.zip` somewhere in `<problem id>/` and `cd` into the extracted directory.
2. Run `pip install -U -r requirements.txt` to install the Python dependencies.
3. Ensure [`fd`](https://github.com/sharkdp/fd), [`zip`](https://linux.die.net/man/1/zip), and [`ld`](https://linux.die.net/man/1/ld) are on your `PATH`.
4. Check which target was used to generate the solution in the summary below, referred to as `<target>` in the next steps.
5. Build the `<target>` CMake target using GCC.
5. Execute the generated `<target>` executable using `<problem id>` as command-line argument.
6. The solution will be generated to `results/<target>/<problem id>.json`.

## Summary

Total score: $$TOTAL_SCORE$$

| Problem | Score | Target |
|---------|-------|--------|
    """.strip() + "\n"

    with temporary_file() as zip_file_path:
        with ZipFile(zip_file_path, "w") as zip_file:
            total_score = 0

            for problem_id in sorted(submissions_by_problem.keys()):
                best_submission = sorted(submissions_by_problem[problem_id], key=lambda submission: submission.score, reverse=True)[0]

                readme_content += f"| {problem_id:,.0f} | {best_submission.score:,.0f} | {best_submission.target} |\n"
                total_score += best_submission.score

                zip_file.write(problems_dir / f"{problem_id}.json", f"problems/{problem_id}.json")
                zip_file.write(submissions_dir / str(best_submission.id) / "solution.json", f"{problem_id}/solution.json")
                zip_file.write(submissions_dir / str(best_submission.id) / "source.zip", f"{problem_id}/source.zip")

            zip_file.writestr("README.md", readme_content.replace("$$TOTAL_SCORE$$", f"{total_score:,.0f}"))

        return send_file(zip_file_path, download_name="camel_case.zip", mimetype="application/zip", as_attachment=True)

@app.get("/submissions/<int:id>/solution")
@auth.login_required(role="admin")
def download_solution(id: int) -> ResponseReturnValue:
    return send_file(submissions_dir / str(id) / "solution.json", as_attachment=True)

@app.get("/submissions/<int:id>/source")
@auth.login_required(role="admin")
def download_source(id: int) -> ResponseReturnValue:
    return send_file(submissions_dir / str(id) / "source.zip", as_attachment=True)

@app.get("/problems/<int:id>/solution")
@auth.login_required(role="submitter")
def download_best_solution(id: int) -> ResponseReturnValue:
    last_submission = db.session.execute(db.select(Submission).filter_by(problem_id=id).order_by(Submission.created_at.desc())).first()
    if last_submission is None:
        abort(404)

    return send_file(submissions_dir / str(last_submission[0].id) / "solution.json", as_attachment=True)

@app.get("/scores")
@auth.login_required(role="submitter")
def scores() -> ResponseReturnValue:
    scores = {}

    submissions = [row[0] for row in db.session.execute(db.select(Submission)).all()]
    for submission in submissions:
        problem_id = submission.problem_id
        score = submission.score

        if problem_id not in scores or score > scores[problem_id]:
            scores[problem_id] = score

    return scores

@app.post("/submit")
@auth.login_required(role="submitter")
def submit() -> ResponseReturnValue:
    problem_id = int(request.form["problemId"])
    score = int(request.form["score"])
    target = request.form["target"]
    solution_file = request.files["solutionFile"]
    source_archive = request.files["sourceArchive"]

    last_submission = db.session.execute(db.select(Submission).filter_by(problem_id=problem_id).order_by(Submission.created_at.desc())).first()
    if last_submission is not None and last_submission[0].score >= score:
        return {"new_best": False, "best_score": last_submission[0].score}

    submission = Submission(problem_id=problem_id, score=score, target=target)
    db.session.add(submission)
    db.session.commit()

    submission_dir = submissions_dir / str(submission.id)
    submission_dir.mkdir(exist_ok=True)
    solution_file.save(submission_dir / "solution.json")
    source_archive.save(submission_dir / "source.zip")

    response = requests.post("https://api.icfpcontest.com/submission", json={
        "problem_id": problem_id,
        "contents": (submission_dir / "solution.json").read_text(encoding="utf-8"),
    }, headers={"Authorization": f"Bearer {icfpc_token}"})

    if last_submission is not None:
        improvement = f"{last_submission[0].score:,.0f} → {score:,.0f}"
    else:
        improvement = f"{score:,.0f}"

    webhook = DiscordWebhook(url=discord_webhook, rate_limit_retry=True)

    if not response.ok:
        webhook.set_content(f"Received HTTP {response.status_code} while submitting new best score on problem {problem_id} ({improvement}, submission id {submission.id})")
        webhook.execute()
        abort(response.status_code)

    embed = DiscordEmbed(title=f"New best score on problem {problem_id}")
    embed.add_embed_field(name="Problem", value=str(problem_id))
    embed.add_embed_field(name="Score", value=improvement)
    embed.add_embed_field(name="Target", value=target)
    embed.set_timestamp()

    webhook.add_embed(embed)
    webhook.execute()

    return {"new_best": True, "best_score": score}

if __name__ == "__main__":
    app.run(debug=True)
