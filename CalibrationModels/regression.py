import numpy as numpy
import pandas as pd
import scipy.stats as stats
import matplotlib.pyplot as plt
import sklearn
import sklearn.cross_validation
import seaborn as sns
import pickle
from sklearn.metrics import mean_squared_error


from sklearn.linear_model import LinearRegression

datafile = input("Please input data file: ")
# CalPurple.csv
data = pd.read_csv(datafile)

X = data.drop('EPA 2.5', axis = 1)
X = X.drop('time', axis = 1)
Y = data['EPA 2.5']

# Segments data into testing and training
X_train, X_test, Y_train, Y_test = sklearn.cross_validation.train_test_split(X, Y, test_size = 0.33, random_state = 5)

# Trains model
lm = LinearRegression()
lm.fit(X_train, Y_train)

# Saves trained model 
# filename = 'test_model.sav'
filename = 'test_model.pckl'
filename = input("Please input model name with .pckl extension: ")
pickle.dump(lm, open(filename, 'wb'))


# Formats new test data into right format for regression model
d = {'Purple 2.5': [24], 'tmp': [91.03], 'hmd': [41.93]}
df = pd.DataFrame(data=d)

# Loads saved model
loaded_model = pickle.load(open(filename, 'rb'))
result = loaded_model.score(X_test, Y_test)

# Test case for inputting new data into model
test_pred = loaded_model.predict(df)

x1, y1 = [0, 25], [0, 25]
plt.plot(x1, y1,marker = 'o')

# Plots predition labels vs. actual labels from saved model
Y_pred = loaded_model.predict(X_test)
plt.scatter(Y_test, Y_pred)
plt.xlabel("EPA PM: $Y_i$")
plt.ylabel("Predicted PM: $\hat{Y}_i$")
plt.title("EPA PM vs Predicted PM: $Y_i$ vs $\hat{Y}_i$")

print("Mean squared error: " + str(mean_squared_error(Y_pred, Y_test)))

plt.show()



