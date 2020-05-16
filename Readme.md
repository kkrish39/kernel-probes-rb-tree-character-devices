Name: Keerthivasan Krishnamurthy
ASU ID: 1217109062

The project contains two parts and each part I made it as a standalone component. There will be a separate test file for each part.

##################

For part 1:

Commands to execute:
 - Compile the program using "Make" command.
 - Move the rbt530_drv.ko and rbt530_tester file to the board.
 - Execute "insmod rbt530_drv.ko"
 - To perform test, run "./rbt530_tester"

Sample output:

Writing Data: key-676, Data-248800933
Reading data: Key-1909   Data-2124440371
Reading data: Key-1895   Data-366355591
Performing IOCTL: Read Order-0
Performing IOCTL: Read Order-0
Writing Data: key-91, Data-1679290923
					***There is no next/previous data***
					***There is no next/previous data***
Performing IOCTL: Read Order-1
					***There is no next/previous data***
Reading data: Key-1962   Data-508924588
Performing IOCTL: Read Order-1
					***There is no next/previous data***
Writing Data: key-1881, Data-1323064093
Reading data: Key-1962   Data-508924588
Reading data: Key-1943   Data-167427038
					***There is no next/previous data***
					***There is no next/previous data***
Performing IOCTL: Read Order-0
.
.
.
.
.
Printing Dump for device 1:

Key:22  Data:20142827
Key:77  Data:939433708
Key:85  Data:2137919283
Key:92  Data:199951071
Key:96  Data:298679523
Key:101  Data:655944335
Key:173  Data:160987648
Key:189  Data:1684365695


##################

For part 2:

Commands to execute:
- Compile the program using "Make" command.
- Move the rbt530_drv.ko file from part 1 to the board.
- Move the rbtprobe_drv.ko and rbprobe_tester file to the board.
- To perform test, run "./rbprobe_tester.
- Five threads will be spawn, where one thread registers the probes. The rest of the four threads will
  perform random operations on the tree.

Sample output:

Writing data to the file 3
Performing IOCTL on file 3
There is no next/previous data 
Reading data from file 3 : Key - 1976   Data - 180236099
Performing IOCTL on file 3
Writing data to the file 4
Performing IOCTL on file 4
Writing data to the file 4
Performing IOCTL on file 4
Writing data to the file 4
Writing data to the file 4
				Probe address: 0xd28ef27a 
				Probe pid: 317 
				Probe timestamp: 33852518304 
				*********Parsed Objects************
				Object Key = 1994  Object Data = 1109114443
				Object Key = 1970  Object Data = 1138077420
				Object Key = 1952  Object Data = 103760931
				Object Key = 1846  Object Data = 1994890994
				Object Key = 1835  Object Data = 596649742
				Object Key = 1827  Object Data = 1031273442
				Object Key = 1824  Object Data = 835583658
				Object Key = 1820  Object Data = 1439778062
				Object Key = 1809  Object Data = 1559933010
Performing IOCTL on file 3
Writing data to the file 3
Performing IOCTL on file 3
There is no next/previous data 
There is no next/previous data 
Performing IOCTL on file 3
Writing data to the file 3
Reading data from file 3 : Key - 1976   Data - 180236099
Reading data from file 3 : Key - 1971   Data - 829104721
Performing IOCTL on file 3
Reading data from file 3 : Key - 1976   Data - 180236099
Reading data from file 4 : Key - 1970   Data - 1138077420
Performing IOCTL on file 4
Reading data from file 4 : Key - 1970   Data - 1138077420
Writing data to the file 3
Writing data to the file 3
Writing data to the file 3
Performing IOCTL on file 3
Writing data to the file 3
Performing IOCTL on file 3
Performing IOCTL on file 4