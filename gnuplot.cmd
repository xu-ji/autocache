# Do not run this script directly, used as reference

set term png nocrop enhanced size 640,480 font "arial,12"
set output "my_output.png"

# buckets:
plot "smart_cacher_2000_0.010000_data.txt" using 1:2 with lines lt rgb "black", 
"buckets.txt" using 1:2 with points lt rgb "red"

# bucket granularities:
plot "buckets_gran.txt" using 1:2 with lines lt rgb "black", 
"buckets_gran.txt" using 1:2 with circles lt rgb "black" fs solid

# errors:
set xlabel "Cache Size (== Number of Minibuckets)"
set ylabel "Average Lookup Error"
set xtics add ("1000" 1000)
set xtics add ("2500" 2500)
plot "SmartCacher_use_middle.txt" using 1:2 with points lt rgb "green", 
"OriginalCacher_use_middle.txt" using 1:2 pt 7 ps 1 lc rgb "black"