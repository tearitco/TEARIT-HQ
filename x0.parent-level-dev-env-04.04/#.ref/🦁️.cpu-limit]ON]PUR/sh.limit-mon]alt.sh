while true; do
  ps -C firefox -o pid,%cpu | awk '$2 > 77 {print "Process firefox (PID "$1") exceeds CPU limit: "$2"%"}'
  sleep 300
done
