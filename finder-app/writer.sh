#!/bin/sh 
# using #!/bin/sh will require sudo the give the script read write acess to filesystem
# using #!/bin/bash will not require sudo the give the script read write acess to filesystem

# varriables for input arguments
writefile=$1 #full path to a file
writestr=$2  # text strig to be written to the file

#get number of argument parsed
NUM_OF_ARG=$#

#check for arg are specified
if [ ${NUM_OF_ARG} -eq 2 ]
then
	echo "extracting directory from file path..."	
	FILESDIR=$(dirname ${writefile})	#extract dir name from parsed file path
	FILENAME=$(basename ${writefile})	#extracts file name from the parsed path
	echo "Directory : ${FILESDIR}"
	
	#check if dir is a dir in the filesystem
	if [ -d ${FILESDIR} ]
	then
		echo "Directory exist, Atempting to write file: $FILENAME"
		#create file in the dir
		#touch ${writefile}
		
		#create and write string to file over-writing existing content
		echo ${writestr} > ${writefile} || { echo "Error: Could not write to file."; exit 1; }
		echo "String written"

	else
		#echo "Directory does not exist, creating: $FILESDIR"
		#create dir : -v flag enables verbose and -p flag enables creation of subdirectories
		mkdir -v -p ${FILESDIR} || { echo "Error: Could not create directory."; exit 1; }
		#create file in the dir
		#touch ${writefile}
		
		#create and write string to file over-writing existing content
		echo ${writestr} > ${writefile} || { echo "Error: Could not create directory."; exit 1; }
		echo "String written"
	fi
	
else
	echo "ERROR: Expecting 2 Arguments"
	exit 1
fi
