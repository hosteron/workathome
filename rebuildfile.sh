#!/bin/sh
if [  ! -n "$1" ];then
echo "should be ${0} inputfile"
echo "do not have inputfile"
exit 1;
fi

OUTFILE=$1_tmp
FILENAME=$1
LASTNAME=REBUILDBACKNAME
if [ _"$1" = _"remove" ];then
if [ -f $LASTNAME ];then
rmname=`cat $LASTNAME`
echo "rm -f $rmname"
rm -f $rmname
rm -f $LASTNAME
fi
exit 0
fi

echo $OUTFILE

if [ ! -f $FILENAME ];then
echo "$1 do not exist!"
exit 1
fi

TOTAL=`ls -l $FILENAME | awk '{printf $5}'`
echo $TOTAL
echo "" > $OUTFILE

NUM=0
OLD_IFS=$IFS
IFS=$'\n'
cat $FILENAME | while read i
do
#echo $i
res_start=`echo "$i" | sed -n "/#033.*</p"`
res_end=`echo "$i" | sed -n "/#033\[0m#015/p"`
if [ -n "$res_start" ];then
echo -n "$i" | sed "s/#033.*.</</" >> $OUTFILE
elif [ -n "$res_end" ];then
echo "$i" | sed "s/#033\[0m#015//" | sed "s/^.\{28\}//" >> $OUTFILE
else
echo "$i" >> $OUTFILE
fi
	TMP_SIZE=`echo "$i" | wc -c`
	NUM=$(($NUM+$TMP_SIZE))
	#echo -ne "$(($NUM*100/$TOTAL))\r"
	PER=$(($NUM*100/$TOTAL))
	echo -n "["
	for ((j=1;j<=100;j++))
	do
		if [ $j -lt $PER ];then
		echo -n "="
		elif [ $j -eq $PER ];then
		echo -n ">"
		else
		echo -n " "
		fi
	done
	echo -n "]"
	echo -n "$PER"
	echo -ne "\r"
done
echo ""
IFS=$OLD_IFS
echo $OUTFILE > $LASTNAME
exit 0
