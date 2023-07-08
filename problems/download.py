import requests
from multiprocessing import Pool
from pathlib import Path

def download_problem(id: int, total: int) -> None:
    print(f"Downloading problem {id}/{total}")

    response = requests.get(f"https://cdn.icfpcontest.com/problems/{id}.json")
    response.raise_for_status()

    output_file = Path(__file__).parent / f"{id}.json"
    with output_file.open("wb") as file:
        file.write(response.content)

def main() -> None:
    problem_count_response = requests.get("https://api.icfpcontest.com/problems")
    problem_count_response.raise_for_status()
    problem_count = problem_count_response.json()["number_of_problems"]

    for i in range(1, problem_count + 1):
        pass

    with Pool(4) as pool:
        pool.starmap(download_problem, [(i, problem_count) for i in range(1, problem_count + 1)])

if __name__ == "__main__":
    main()
