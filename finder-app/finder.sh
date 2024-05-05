#!/bin/bash

#varriables
FILESDIR=$1
SEARCHSTR=$2
NUM_OF_ARG=$#

NO_OF_MATCHING_LINES=0
NO_OF_FILES=0

#check for valid arg entry
if [ ${NUM_OF_ARG} -eq 2 ]
then
   #check if dir entered is truly a dir
   if [ -d ${FILESDIR} ]
   then
      #perform the search here
      find . -${SEARCHSTR} ${FILESDIR}
      echo "The number of files are ${NO_OF_FILES} and the number of matching lines are ${NO_OF_MATCHING_LINES}"
      #echo "Is a DIR = ${FILESDIR} and str = ${SEARCHSTR} ARG = ${NUM_OF_ARG}"
      exit 1
   else
      echo "ERROR: First argument does not represent a directory on the filesystem"
    fi
else
   echo "ERROR: Expecting 2 Arguments"
   exit 1
fi
