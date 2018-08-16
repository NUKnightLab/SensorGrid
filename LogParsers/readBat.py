import matplotlib.pylab as plt
import time

# BATTERY LEVEL DATA


bat_occur = []
data = {}                 	# The dictionary where we will store results.
substr = "bat"            	# Substring to use for search.
logfile = input("Please input logfile: ")
csvfile = input("Please input CSV file to write to: ")


def find_between(s, first, last):
    try:
        start = s.index(first) + len(first)
        end = s.index(last, start)
        return s[start:end]
    except ValueError:
        return ''


with open (logfile, 'rt') as in_file:        		# Open file for reading text.
	csv = open(csvfile, "w")		   				# Open csv file to record lods
	csv.write("Timestamp, battery level, time\n")
	for linenum, line in enumerate(in_file):       	# Keep track of line numbers
		if line.lower().find(substr) != -1: 	   	#If case-insensitive substring search matches, then:
			ts = find_between(line, "ts", "}")		# Find timestamp data
			level = find_between(line, "bat", "ts")	# Find battery level
			ts = ts.replace(":",'')
			ts = ts.replace('"','')
			level = level.replace(":",'')
			level = level.replace('"','')
			level = level.replace(',','')
			data[ts] = level
			t = time.strftime('%m-%d %H:%M', time.localtime(int(ts)))
			row = ts + ',' + level + ',' + t + '\n'
			csv.write(row)