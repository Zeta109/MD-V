FROM gcc:13
WORKDIR /app
COPY node.cpp .
RUN g++ -O2 -std=c++17 -o node node.cpp
CMD ["./node"]
