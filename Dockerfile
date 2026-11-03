FROM python:3.11-slim
WORKDIR /app
COPY node.py .
CMD ["python","node.py"]
