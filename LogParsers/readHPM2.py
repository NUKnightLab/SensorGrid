import matplotlib.pylab as plt
import time


# 2.5 HPM DATA

hpm_occur = []  
data = {}					# The dictionary where we will store results.
substr = "hpm"  			# Substring to use for search.
hpmMAX = 40					# Upper limit of feasible HPM readings
#logfile = 'idle.txt'		# Log file to read
logfile = input("Please input logfile: ")
#csvfile = 'idleHPM10.csv'	# CSV file to write data to 
csvfile = input("Please input CSV file to write to: ")

def find_between(s, first, last):
    try:
        start = s.index(first) + len(first)
        end = s.index(last, start)
        return s[start:end]
    except ValueError:
        return ''



with open (logfile, 'rt') as in_file:     			# Open file for reading text.
	csv = open(csvfile, "w")						# Open CSV file to record log
	csv.write("Timestamp, hpm , time\n")
	for linenum, line in enumerate(in_file):    			# Fine lines of relevant data
		if line.lower().find(substr) != -1:     			# If case-insensitive substring search matches, then:
			hpm_occur.append((linenum, line.rstrip('\n')))  # strip linebreaks, store line and line number in list as tuple.
			ts = find_between(line, "ts", ",")				# Find timestamp data
			hpm = find_between(line, "[", ",")				# Find 2.5 HPM data
			ts = ts.replace(":",'')
			ts = ts.replace('"','')
			if ((int(ts) - 7) % 600 == 0):
				if (int(hpm) < hpmMAX):			 
					data[ts] = hpm
					t = time.strftime('%m-%d %H:%M', time.localtime(int(ts)))
					row = ts + ',' + hpm + ',' + t + '\n'
					csv.write(row)  