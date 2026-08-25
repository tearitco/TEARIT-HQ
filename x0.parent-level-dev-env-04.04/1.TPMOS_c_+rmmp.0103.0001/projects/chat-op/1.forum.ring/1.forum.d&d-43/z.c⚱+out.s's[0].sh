#https://superuser.com/questions/666119/how-to-allow-user-to-input-file-names-in-bash-script
#⁉️🦄️🕰️
input=$1
input2=$2
input3=$3
inputDirective1=$4 # << or <
inputDirective2=$5 # << or <
file=${input%.*}
#echo $file 
#> log.out.0.txt
#> log.out.1.txt

#could later add this in code ,
#but will probably put #/#/## style timestamp in clock holder 4 appends

#gcc -S -O2 -o "$file".s "$file".c
#-O2 for optimized(redable) -00  (for none) -OS = no xs code... maybe nice
#gcc -S -m32 -fno-pic -fno-asynchronous-unwind-tables
#https://stackoverflow.com/questions/47039687/c-assembly-language-att-walkthrough
#
 #gcc "$file".c -o "$file" -lm -lpulse-simple -lpulse
 gcc "$file".c -o +x/"$file".+x -lm
 
 
 
 
 # rm "$file".o
 #   ./"$file" #http://query1.finance.yahoo.com/v8/finance/chart/NAUKRI.NS?region=US&lang=en-US&includePrePost=false&interval=2m&useYfid=true&range=1d&corsDomain=finance.yahoo.com&.tsrc=finance

#x.com #$input2  $input3  #ON 4 stdio 64 ch5 but turned off <3
   #cat $input3 #ON 4 stdio 64 ch5 but turned off <3

 echo $?
  # rm "$file"
 # cat log.out.0.txt
 # cat log.out.1.txt
