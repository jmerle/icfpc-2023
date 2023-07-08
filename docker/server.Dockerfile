FROM python:3.11.4

WORKDIR /app

COPY requirements.txt /app/requirements.txt
RUN pip install -U -r requirements.txt

COPY problems /app/problems
COPY server /app/server

EXPOSE 8000

ENTRYPOINT ["gunicorn", "-w", "4", "-b", "0.0.0.0:8000", "server.main:app"]
