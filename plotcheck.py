import pyqtgraph as pg
from pyqtgraph.Qt import QtWidgets

app = QtWidgets.QApplication([])

plot = pg.plot([1,2,3,2,1])

app.exec()