# File Compression Visualizer

A full-stack web application that demonstrates **Huffman Coding** and **LZW (Lempel-Ziv-Welch)** compression algorithms with step-by-step visualization. Built as an ADS (Advanced Data Structures) project.

## Architecture

- **Backend** — C++ REST API using the [Crow](https://crowcpp.org/) web framework
- **Frontend** — React app with GSAP animations for interactive algorithm visualization

## Features

- Upload any file and compress it using Huffman or LZW algorithms
- Visualize the compression process step-by-step:
  - **Huffman**: frequency analysis, tree construction (merge steps), code table generation, and encoding
  - **LZW**: dictionary building, encoding steps, and bit packing
- Decompress data back to the original file
- View compression ratio and size statistics

## Prerequisites

- **g++** with C++17 support
- **libasio-dev** (networking library for Crow)
- **libboost-all-dev**
- **Node.js** and **npm**

Install system dependencies (Ubuntu/Debian):

```bash
sudo apt install g++ libasio-dev libboost-all-dev libssl-dev
```

## Project Structure

```
.
├── server.cpp          # Crow REST API server (compress/decompress endpoints)
├── huffman.h           # Huffman coding implementation
├── lzw.h              # LZW compression implementation
├── Makefile            # Build configuration for the backend
├── Crow/              # Crow framework (git clone)
└── frontend/          # React frontend
    └── src/
        ├── App.js
        └── components/
            ├── HuffmanViz.js      # Huffman visualization
            ├── LzwViz.js          # LZW visualization
            └── NetworkCanvas.js   # Canvas-based network animation
```

## Getting Started

### 1. Build the backend

```bash
make
```

### 2. Install frontend dependencies

```bash
cd frontend
npm install
```

### 3. Run the application

Start the backend server:

```bash
make run
# Server runs at http://localhost:18080
```

In a separate terminal, start the React dev server:

```bash
cd frontend
npm start
# Frontend runs at http://localhost:3000 (proxies API calls to :18080)
```

For production, build the frontend and serve it from the backend:

```bash
cd frontend
npm run build
# The backend serves frontend/build/index.html at http://localhost:18080
```

## API Endpoints

### `POST /api/compress`

Compress file data using the specified algorithm.

**Request body:**
```json
{
  "algorithm": "huffman" | "lzw",
  "data": "<base64-encoded file data>",
  "filename": "example.txt"
}
```

**Response:** compressed data (base64), compression ratio, size stats, and visualization steps.

### `POST /api/decompress`

Decompress previously compressed data.

**Request body:**
```json
{
  "algorithm": "huffman" | "lzw",
  "data": "<base64-encoded compressed data>"
}
```

**Response:** decompressed data (base64), size stats, and visualization steps.
