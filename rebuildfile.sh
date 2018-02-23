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

echo "" > $OUTFILE

NUM=0
cat $FILENAME | while read i
do
#echo $i
res_start=`echo $i | sed -n "/#033.*</p"`
res_end=`echo $i | sed -n "/#033\[0m#015/p"`
if [ -n "$res_start" ];then
echo -n $i | sed "s/#033.*.</</" >> $OUTFILE
elif [ -n "$res_end" ];then
echo $i | sed "s/#033\[0m#015//" | sed "s/^.\{28\}//" >> $OUTFILE
else
echo $i >> $OUTFILE
fi
done
echo $OUTFILE > $LASTNAME
exit 0
