# a4

for when storing ensure to convert back to host byte order

task2:
	need to allocate enough memory and fill var with fread, then can access like array

hd <image>			 # hex dump

Task 3:
	1. find root
	2. look for files (file bit set)
		2.1 find the right file by matching name
		2.2 if not found WRITE TO stderr
	3. Read start block and number of blocks
	4. Read block
	5. Can use fputc to send char directly to stdout

NextBlockStart = FatStart + CurrentBlockStart * 4 (each FAT entry is 4 bytes)