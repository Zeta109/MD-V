FROM gcc:12-bookworm
WORKDIR /app
COPY node.cpp .
RUN g++ -O2 -pthread -o node node.cpp
CMD ["./node"]
