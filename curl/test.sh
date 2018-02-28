echo $[$(date +%s%N)/1000000]
cat test.txt  | curl -d /dev/fd/0 -X POST http://121.196.193.71:8086/DebugFileData/UploadFile
