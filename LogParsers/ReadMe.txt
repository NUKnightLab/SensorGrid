To use:

First, put all the log files from one period into a single directory
In the command line, write “cat *.txt > name.txt” to merge all files into one (Where name is whatever you want to call it)
Place the parser you want to use in the directory that holds the new merged log file 

	- readBat.py records the battery level
	- readHPM.py records both 2.5 and 10 particulate matter
	- readHPM2.py records just 2.5 pm
	- readHPM10.py records just 10 pm

The HPM parsers only record values under a specific hpmMAX value that can be changed in the code if wanted 

Once you run the scraper, you will be asked to input the log file to parse, which will be called what you named it earlier. Then you will be asked to input the name of the CSV to write to such as batterylevel.csv

The data will then be formatted in the csv as:
	- Timestamp, Battery, HPM 2.5, HPM 10, Time	
What columns show up depends on which parser you use. Time stamp is an epoch time, while time is formatted as MM-DD Hour:Minute

In excel, you can use the scatter plot to visualize the data. Or use Flourish to best visualize and compare the data


