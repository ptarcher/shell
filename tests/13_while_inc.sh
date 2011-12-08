#!/bin/sh
i="0"

echo start while loop
while [ $i -lt 5 ]
do
    echo $i
    i="7"
    sleep 1
done

echo done while loop
