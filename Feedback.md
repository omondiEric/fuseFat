# Lab 2a: directory creation and navigation
__Score: 25/25__

 * __Correctness/Functionality:__
   * _5/5_: initializing a fresh file system
     * create `.` and `..` inside `/`
   * _5/5_: reading directories
     * report accurate contents (empty and populated directories)
     * reasonable contents for `getattr` (as illustrated by `ls -l`)
   * _5/5_: creating directories
     * can create directories in `/`
     * can create subdirectories
     * can create directories with (reasonably) large names
     * _Not tested:_ directory size or depth limits
     * _Not tested:_ accuracy of link counts
   * _5/5_: traversing directories
     * can navigate to directories and subdirectories (e.g., path lookup)
   * _5/5_: persistence
     * data persists after unmounting

## General Comments
Nice job on this lab! Some general comments below, but this is great work.
 * Try to avoid using magic numbers in your code. Especially if they rely on the sizes of structs (like struct dir_ent). What if the alignment or the integer size is different than your assumptions?
 * You repeatedly scan through a cluster to find a target dirent. This could be factored into a separate method.
 * To avoid opening/closing the disk repeatedly, you could open it once in init and close it once in destroy. You can use fsync to force writes to be persistent.
 * Even though you are interacting with the "disk" using the file interface, you probably want your FAT to be sector-aligned.

## Test: can you mount a fresh FS?
(I ran ./fat -s -f /home/cs333/test/mnt in another window)

### comments:
  * Mounting had no errors.



## Test: long listing of empty directory (should see '.' and '..')
```
ls -la /home/cs333/test/mnt
	total 4
	drwxr-xr-x 0 root root 4096 Dec 31 1969 .
	drwxr-xr-x 3 cs333 cs333 4096 Apr 23 13:49 ..
```
### comments:
  * `.` and `..` look correctly initialized
  * readdir appears to work


## Test: making one directory
```
mkdir /home/cs333/test/mnt/dir1
ls -la /home/cs333/test/mnt
	total 4
	drwxr-xr-x 0 root root 4096 Dec 31 1969 .
	drwxr-xr-x 3 cs333 cs333 4096 Apr 23 13:49 ..
	drwxr-xr-x 0 root root 4096 Dec 31 1969 dir1
ls -la /home/cs333/test/mnt/dir1
	total 0
	drwxr-xr-x 0 root root 4096 Dec 31 1969 .
	drwxr-xr-x 0 root root 4096 Dec 31 1969 ..
```
### comments:
  * `dir1` is properly initialized with `.` and `..`
  * The parent directory reflects the update (includes `dir1`)



## Test: making one directory, then subdirectory
```
mkdir /home/cs333/test/mnt/dir1
ls -la /home/cs333/test/mnt
	total 4
	drwxr-xr-x 0 root root 4096 Dec 31 1969 .
	drwxr-xr-x 3 cs333 cs333 4096 Apr 23 13:49 ..
	drwxr-xr-x 0 root root 4096 Dec 31 1969 dir1
mkdir /home/cs333/test/mnt/dir1/subdir1
ls -la /home/cs333/test/mnt/dir1
	total 0
	drwxr-xr-x 0 root root 4096 Dec 31 1969 .
	drwxr-xr-x 0 root root 4096 Dec 31 1969 ..
	drwxr-xr-x 0 root root 4096 Dec 31 1969 subdir1
```
### comments:
  * You correctly detect the already existing directory
  * The subdirectory appears in `dir1` as expected



## Test: making directory with moderately large name
```
mkdir /home/cs333/test/mnt/1234567890123456789
ls -la /home/cs333/test/mnt
	total 4
	drwxr-xr-x 0 root root 4096 Dec 31 1969 .
	drwxr-xr-x 3 cs333 cs333 4096 Apr 23 13:49 ..
	drwxr-xr-x 0 root root 4096 Dec 31 1969 1234567890123456789
	drwxr-xr-x 0 root root 4096 Dec 31 1969 dir1
```
### comments:
  * Long(ish) name works as expected
  * 20 characters is not super long, but it's reasonable. Correct behavior.


## Test: do contents survive unmount?
```
fusermount -u /home/cs333/test/mnt
```

(I ran ./fat -s -f /home/cs333/test/mnt in another window)

```
ls -la /home/cs333/test/mnt
	total 4
	drwxr-xr-x 0 root root 4096 Dec 31 1969 .
	drwxr-xr-x 3 cs333 cs333 4096 Apr 23 13:49 ..
	drwxr-xr-x 0 root root 4096 Dec 31 1969 1234567890123456789
	drwxr-xr-x 0 root root 4096 Dec 31 1969 dir1
```
### comments:
  * All directories look persistent!


