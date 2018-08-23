import matplotlib.pylab as plt
import time


# Humidity

hmd_occur = []  
data = {}					# The dictionary where we will store results.
substr = "hmd"  			# Substring to use for search.
logfile = input("Please input logfile: ")
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
	csv.write("Timestamp, hmd, time\n")
	for linenum, line in enumerate(in_file):    			# Fine lines of relevant data
		if line.lower().find(substr) != -1:     			# If case-insensitive substring search matches, then:
			hmd_occur.append((linenum, line.rstrip('\n')))  # strip linebreaks, store line and line number in list as tuple.
			ts = find_between(line, "ts", "}")				# Find timestamp data
			hmd = find_between(line, "hmd", ",")			# Find humidity data 
			hmd = hmd.replace('":','')			
			ts = ts.replace('":','')
			ts = ts.replace('}','')
			
			if ((int(ts) - 7) % 3600 == 0):					# Records hourly	 
				t = time.strftime('%m-%d %H:%M', time.localtime(int(ts)))
				row = ts + ',' + hmd + ',' + t + '\n'
				csv.write(row)