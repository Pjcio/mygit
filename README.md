# MyGit

## Introduction
Personal C++ project aimed at replicating some of Git's main functionalities. The goal is to understand Git's internal workings by implementing commands for managing blobs, trees, and commits, as well as allowing simple repository cloning.

---

## Steps Status

### 1. Initialize the .git directory ✅
Creation of the `.git` directory and standard subdirectories (`objects`, `refs`, etc.) implemented.

### 2. Read a blob object ✅
It is possible to correctly read a blob object from the repository and display its content using `mygit cat-file -p <sha1>`.

### 3. Create a blob object ✅
Blob creation is implemented.  
Files can be converted into blob objects and stored in the repository using `mygit hash-object [options] <file>`.

### 4. Read a tree object ⬜
To be completed.

### 5. Write a tree object ⬜
To be completed.

### 6. Create a commit ⬜
To be completed.

### 7. Clone a repository ⬜
To be completed.
