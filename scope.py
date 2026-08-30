import numpy as np
import pyqtgraph as pg
from pyqtgraph.Qt import QtWidgets
from math import hypot

class Scope:
    

    def perpendicular_distance(self, p, a, b):
        """Abstand Punkt p zur Strecke a-b"""

        if a == b:
            return hypot(p[0]-a[0], p[1]-a[1])

        x0, y0 = p
        x1, y1 = a
        x2, y2 = b

        num = abs(
            (y2-y1)*x0
            - (x2-x1)*y0
            + x2*y1
            - y2*x1
        )

        den = hypot(x2-x1, y2-y1)

        return num / den


    def douglas(self, points, epsilon):

        if len(points) < 3:
            return points

        max_dist = 0
        index = 0

        start = points[0]
        end = points[-1]

        for i in range(1, len(points)-1):

            d = self.perpendicular_distance(
                points[i],
                start,
                end
            )

            if d > max_dist:
                max_dist = d
                index = i

        if max_dist > epsilon:

            left = self.douglas(points[:index+1], epsilon)
            right = self.douglas(points[index:], epsilon)

            return left[:-1] + right

        else:

            return [start, end]
        
    def __init__(self):

        self.N = 1000

        self.gx = np.zeros(self.N)
        self.gy = np.zeros(self.N)
        self.gz = np.zeros(self.N)

        self.win = pg.GraphicsLayoutWidget(
            title="Magic Wand IMU"
        )

        self.plot = self.win.addPlot()

        self.win = pg.GraphicsLayoutWidget(
            title="Magic Wand IMU"
        )

        self.win.resize(1200, 800)

        self.plot = self.win.addPlot()

        self.plot.setYRange(-4000, 4000)

        self.curve_gx = self.plot.plot(
            pen='r',
            name='gx'
        )

        self.curve_gy = self.plot.plot(
            pen='g',
            name='gy'
        )

        self.curve_gz = self.plot.plot(
            pen='b',
            name='gz'
        )

        self.timer = pg.QtCore.QTimer()
        self.timer.timeout.connect(self.update)
        self.timer.start(50)

        self.win.show()

        self.projection_x = []
        self.projection_y = []

        self.projection_win = pg.GraphicsLayoutWidget(
            title="Magic Wand Projection (1 m)",
        )
        self.projection_win.resize(800, 800)
        self.projection_plot = self.projection_win.addPlot()
        self.projection_plot.setAspectLocked(True)
        self.projection_plot.setXRange(-1.1, 1.1)
        self.projection_plot.setYRange(-1.1, 1.1)
        self.projection_plot.setLabel("bottom", "x", units="m")
        self.projection_plot.setLabel("left", "z", units="m")
        self.projection_plot.showGrid(x=True, y=True)
        self.projection_trace = self.projection_plot.plot(
            pen=pg.mkPen("w", width=2),
        )
        self.projection_trace_simplified = self.projection_plot.plot(
            pen=pg.mkPen("y", width=2),
        )
        self.projection_point = self.projection_plot.plot(
            pen=None,
            symbol="o",
            symbolSize=12,
            symbolBrush="r",
        )
        self.projection_win.show()

    def add_sample(self, gx, gy, gz):

        self.gx[:-1] = self.gx[1:]
        self.gy[:-1] = self.gy[1:]
        self.gz[:-1] = self.gz[1:]

        self.gx[-1] = gx
        self.gy[-1] = gy
        self.gz[-1] = gz

    def add_projection_point(self, x, z):
        self.projection_x.append(x)
        self.projection_y.append(z)

    def clear_xy(self):
        self.projection_x = []
        self.projection_y = []

    def update(self):

        self.curve_gx.setData(self.gx)
        self.curve_gy.setData(self.gy)
        self.curve_gz.setData(self.gz)
        self.projection_trace.setData(self.projection_x, self.projection_y)

        self.simplified = self.douglas(list(zip(self.projection_x, self.projection_y)), 0.1)
        self.projection_trace_simplified.setData(
            [p[0] for p in self.simplified],
            [p[1] for p in self.simplified],
        )

        if self.projection_x:
            self.projection_point.setData(
                [self.projection_x[-1]],
                [self.projection_y[-1]],
            )
