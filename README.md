## BabyDB

This is the codebase for Database System course, IIIS, Tsinghua University in 2025.

You should start from the branch `main`, and when each project posted, this branch will be updated. So before you start your next project, confirm your code is up to date.

**Important Notice: You should not make your code public anywhere.**

### Cloning this Repository

1. Go [here](https://github.com/new) to create a new empty (without README.md) repository under your account. Pick a name (e.g. `babydb-private`) and select **Private** for the repository visibility level.
2. Create a bare clone of the repository:

```
$ git clone --bare https://github.com/hehezhou/babydb.git
```

3. Mirror-push to the new repository:

```
$ cd babydb.git
$ git push https://github.com/USERNAME/babydb-private.git main
```

And remove the filefolder:

```
$ cd ..
$ rm babydb.git -rf
```

4. Clone your private repository to your machine:

```
$ git clone https://github.com/USERNAME/babydb-private.git
$ cd babydb-private
```

5. Add the codebase BabyDB repository as a second remote, which allows you to retrieve changes from the codebase repository and merge them into your repository:

```
$ git remote add codebase https://github.com/hehezhou/babydb.git
```

You can verify that the remote was added with the following command:

```
$ git remote -v
codebase        https://github.com/hehezhou/babydb.git (fetch)
codebase        https://github.com/hehezhou/babydb.git (push)
origin ...
```

You can now pull in changes from the codebase repository as needed with:

```
$ git pull codebase main
```

We suggest you working on your projects in separate branches. If you do not understand how Git branches work, [learn how](https://git-scm.com/book/en/v2/Git-Branching-Basic-Branching-and-Merging).

### Build

**We recommend you run this code on Linux.** If you're using Windows, MacOS or other systems, and fails to build BabyDB, you should first try to install a Linux virtual machine, or WSL2 on Windows. If you still have problems, find TAs' help.

Before start building, make sure you have `make`, `cmake` and C++ compiler (`g++` or `clang++`) on your machine and they are added into your environment variable, which means you can directly use them on your shell.

Then run the following commands to build the system:

```
$ mkdir build
$ cd build
$ cmake ..
$ make -j`nproc`
```

If you want to compile the system in release mode, pass in the following flag to cmake:

```
$ cmake -DCMAKE_BUILD_TYPE=Release ..
$ make -j`nproc`
```

Otherwise by default the system will be compiled in debug mode. We recommend you to develop and debug on debug mode, but test performance on release mode.

By default, debug mode will use address sanitizer. If you want to use other sanitizers like undefined sanitizer, pass in the following flag:

```
$ cmake -DBABYDB_SANITIZER=undefined ..
$ make -j`nproc`
```

Then you can run the tests:

```
$ make check-tests
```

or get more details:

```
$ make check-tests-details
```

You may fail on some tests. If their name are all start with `Project`, don't worry, they will be passed if you correctly complete your project. But if you fail on other tests, please first check your steps, and then contact with the TA.

### Project 1

You should only modify `src/storage/art.cpp` and `src/include/storage/art.hpp`, and pass all `Project1ArtIndexMVCC` tests if you complete the whole project.

**Notice:** The basic version does not store row id in the index, but in the final version, `LookupKey` and `ScanRange` should returns row id instead of key itself. You can choose a reasonable point to implement it. (Suggestion: When implementing the version link.)

At the start of `src/storage/art.cpp`, there are some tips.

To submit, run the following code to generate the submission file (make sure `zip` is in your environment variable):

```
make submit-p1
```

then manually add your report into the zip file, and submit to Web Learning.

Here's some information about tests:

For tests start with `SortedKeys`, the key is equal to the row id, so you can pass it without storing row id.

For each test, you'll get the whole score if you pass the test within reasonable time.

1. SortedKeys_RangeQuery: Require task 1.
2. SortedKeys_RangeQuery_MultipleRanges: Require task 1. Note that this test have a large number of range queries with a small range.
3. RandomKeys_OnlyPointQuery: No addition requirement. (Only require returning correct row id.)
4. RandomKeys_RangeQuery: Require task 1.
5. SparseKeys_OnlyPointQuery: No addition requirement.
6. SparseKeys_RangeQuery: Require task 1.
7. DenseKeys_WithUpdates_PointQuery: Require task 2.
8. MixedReadWrite_HighQueryRatio: Require task 2.
9. LongVersionChain_SequentialTs: Require task 2. Note that this test have a large number of version link update.
10. LongVersionChain_RandomTs: Require task 3. Note that this test have a large number of version link update.
11. LongVersionChain_SequentialTs_RangeQuery: Require task 1 & 2. Note that this test have a large number of version link updates and range queries.
12. LongVersionChain_RandomTs_RangeQuery: Require ALL task. Note that this test have a large number of version link updates and range queries.
