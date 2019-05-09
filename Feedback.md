# Lab 2b: files, directories, and deletion
__Overall Score: 63.5/80__

After your final project, if you would like to take some time to
debug/finish your implementation, I am happy to work with you! I would
like to give you more credit for your work since it is obvious that you
put a lot of time into this project, and your are not far from there.


### Directories (Part 2a functionality)
   * _5/5_: initializing a fresh file system (creates `.` and `..` inside `/`)
   * _5/5_: reading directories (readir)
   * _5/5_: creating directories in root 
   * _5/5_: creating and traversing subdirectories
   * _5/5_: persistence (data persists after unmounting)

### Small File (fit within one cluster) Tests
   * _5/5_: create small files
   * _5/5_: Extend the small files by appending to the end (seek to end + write)
   * _4.5/5_: Overwrite bytes in the middle of the file in the middle (no change to file size).
     * Bad return value from system call (error), but correct behavior otherwise
   * _4/5_: Truncate a one-cluster files to a smaller size.
     * After some fixes this works; your code was close-to-correct.
     * My fixes didn't complete the truncate implementation, but they should help to debug
 
### Large file (extend beyond one cluster) tests
   * _2/5_: Create a large file (the file spans multpile clusters)
     * Your files can be 4096 bytes at most
     * Like truncate, your code appears close-to-correct, but debugging this was not easy.
     * If you modify the parameters during your function (e.g., offset, size), your return value calculation is off.
   * _2/5_: Truncate the file to a smaller size (smaller size but the same number of clusters)
   * _2/5_: Truncate the file to a smaller size (smaller size and a smaller number of clusters)


### Deletion
   * _0/5_: Delete a file (unlink)
   * _5/5_: Delete a non-empty directory (should *not* actually delete the directory)
   * _5/5_: Delete an empty directory (rmdir)

### Style/Organization
   * _4/5_
