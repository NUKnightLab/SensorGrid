import matplotlib.pylab as plt
import time


# 2.5 and 10 HPM DATA

hpm_occur = []  
data = {}					# The dictionary where we will store results.
substr = "hpm"  			# Substring to use for search.
hpmMAX = 40					# Upper limit of feasible HPM readings
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
	csv.write("Timestamp, hpm 2.5, hpm 10, time\n")
	for linenum, line in enumerate(in_file):    			# Fine lines of relevant data
		if line.lower().find(substr) != -1:     			# If case-insensitive substring search matches, then:
			hpm_occur.append((linenum, line.rstrip('\n')))  # strip linebreaks, store line and line number in list as tuple.
			ts = find_between(line, "ts", ",")				# Find timestamp data
			hpm2 = find_between(line, "[", ",")				# Find 2.5 HPM data
			hpm10 = find_between(line, "hpm", "}")
			hpm10 = find_between(hpm10, ",", "]")			# Find 10 HPM data
			ts = ts.replace(":",'')
			ts = ts.replace('"','')
			
			if ((int(ts) - 7) % 600 == 0):
				if ((int(hpm2) < hpmMAX) & (int(hpm10) < hpmMAX)):			 
					t = time.strftime('%m-%d %H:%M', time.localtime(int(ts)))
					row = ts + ',' + hpm2 + ',' + hpm10 + ',' + t + '\n'
					csv.write(row)