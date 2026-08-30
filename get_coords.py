#!/usr/bin/env python3

import math
import sys
from PySide6.QtCore import Qt, QPointF
from PySide6.QtGui import (
    QAction,
    QColor,
    QPainter,
    QPen,
    QPixmap,
)
from PySide6.QtWidgets import (
    QApplication,
    QFileDialog,
    QGraphicsPixmapItem,
    QGraphicsScene,
    QGraphicsView,
    QMainWindow,
    QLabel,
)


class ImageView(QGraphicsView):
    # position_list
    positions = []

    def __init__(self, status_label):
        super().__init__()

        self.status_label = status_label

        self.scene = QGraphicsScene(self)
        self.setScene(self.scene)

        self.pixmap_item = None
        self.image = None

        self.setMouseTracking(True)

        self.setRenderHints(QPainter.Antialiasing |
                            QPainter.SmoothPixmapTransform)

        # Dunkler Hintergrund
        self.setBackgroundBrush(QColor(45, 45, 45))

        # Zoom um Maus
        self.setTransformationAnchor(QGraphicsView.AnchorUnderMouse)
        self.setResizeAnchor(QGraphicsView.AnchorUnderMouse)

    def load_image(self, filename):
        pix = QPixmap(filename)
        if pix.isNull():
            return

        self.scene.clear()
        self.pixmap_item = QGraphicsPixmapItem(pix)
        self.scene.addItem(self.pixmap_item)
        self.scene.setSceneRect(pix.rect())

        self.resetTransform()
        self.fitInView(self.pixmap_item, Qt.KeepAspectRatio)

        self.image = pix

    def wheelEvent(self, event):
        factor = 1.25 if event.angleDelta().y() > 0 else 0.8
        self.scale(factor, factor)

    def mouseMoveEvent(self, event):
        if self.pixmap_item is None:
            return

        pos = self.mapToScene(event.position().toPoint())

        x = int(pos.x())
        y = int(pos.y())

        if (
            0 <= x < self.image.width()
            and 0 <= y < self.image.height()
        ):
            self.status_label.setText(f"X={x}   Y={y}")

        super().mouseMoveEvent(event)

    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton:
            pos = self.mapToScene(event.position().toPoint())

            x = int(pos.x())
            y = int(pos.y())
            self.positions.append((x, y))

            if (
                0 <= x < self.image.width()
                and 0 <= y < self.image.height()
            ):
                text = f"{x},{y}"
                QApplication.clipboard().setText(text)
                self.status_label.setText(
                    f"X={x}   Y={y}   (kopiert)"
                )
        if event.button() == Qt.RightButton:
            # calculate the length of the path drawn by the user
            if len(self.positions) > 1:
                length = 0
                for i in range(1, len(self.positions)):
                    x0, y0 = self.positions[i - 1]
                    x1, y1 = self.positions[i]
                    dx = x1 - x0
                    dy = y1 - y0
                    length += (dx ** 2 + dy ** 2) ** 0.5
                # calculate relative length of each segment
                relative_lengths = []
                for i in range(1, len(self.positions)):
                    x0, y0 = self.positions[i - 1]
                    x1, y1 = self.positions[i]
                    dx = x1 - x0
                    dy = y1 - y0
                    segment_length = (dx ** 2 + dy ** 2) ** 0.5
                    relative_length = segment_length / length
                    relative_lengths.append(relative_length)
                # calculate the angle of each segment
                angles = []
                for i in range(1, len(self.positions)):
                    x0, y0 = self.positions[i - 1]
                    x1, y1 = self.positions[i]
                    dx = x1 - x0
                    dy = y1 - y0
                    angle = math.degrees(math.atan2(dy, dx))
                    angle += 90  # adjust angle to match the coordinate system
                    angle = (angle + 360) % 360  # normalize angle to [0, 360)
                    angles.append(angle)
                # create a string representation of the path
                # {
                #    "spell": "abcd",
                #    "segments": [
                #        { "dir": 140, "l": 222 },
                #        { "dir": 45,  "l": 333 },
                #        { "dir": 245, "l": 445 }
                #    ]
                #}
                text = "{\n"
                text += '    "spell": "abcd",\n'
                text += '    "segments": [\n'
                for i in range(len(angles)):
                    text += f'        {{ "dir": {angles[i]:.0f}, "l": {relative_lengths[i]*1000:.0f} }},\n'
                text += '    ]\n'
                text += "}\n"
                QApplication.clipboard().setText(text)
            self.positions.clear()

        super().mousePressEvent(event)

    def drawForeground(self, painter, rect):
        # Fadenkreuz
        pos = self.mapToScene(self.mapFromGlobal(self.cursor().pos()))

        painter.setPen(QPen(QColor(255, 0, 0), 0))

        painter.drawLine(
            QPointF(rect.left(), pos.y()),
            QPointF(rect.right(), pos.y())
        )

        painter.drawLine(
            QPointF(pos.x(), rect.top()),
            QPointF(pos.x(), rect.bottom())
        )


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()

        self.setWindowTitle("Bildkoordinaten")

        self.status = QLabel("Kein Bild")
        self.statusBar().addPermanentWidget(self.status)

        self.view = ImageView(self.status)
        self.setCentralWidget(self.view)

        open_action = QAction("&Öffnen", self)
        open_action.triggered.connect(self.open_image)

        self.menuBar().addAction(open_action)

        self.resize(1200, 900)

    def open_image(self):
        filename, _ = QFileDialog.getOpenFileName(
            self,
            "Bild öffnen",
            "",
            "Bilder (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"
        )

        if filename:
            self.view.load_image(filename)


app = QApplication(sys.argv)

w = MainWindow()

if len(sys.argv) > 1:
    w.view.load_image(sys.argv[1])

w.show()

sys.exit(app.exec())