#!/bin/sh
if [  ! -n "$1" ];then
echo "should be ${0} inputfile"
echo "do not have inputfile"
exit -1;
fi

OUTFILE=$1_tmp
FILENAME=$1
echo $OUTFILE

if [ ! -f $FILENAME ];then
echo "$1 do not exist!"
exit -1
fi

echo "" > $OUTFILE

NUM=0
cat $FILENAME | while read i
do
echo $i
#NUM=$(($NUM+1))
##echo $NUM
#if [[ $NUM -gt 10 ]];then
#exit -1
#fi

done
