#pragma once

#include "PipeWireGraphState.h"

#include <QHash>
#include <QPointF>

// Automatic patchbay layout. Connected nodes go in columns following the signal (players and
// microphones, this app's filters, output devices, recorders), each column ordered by its
// neighbours so cables cross as little as possible. Nodes without any link are parked in the
// column right of everything, stacked from the top, so they stay out of the way.
namespace v9::pw {

inline constexpr qreal LayoutColumnWidth = 300;
inline constexpr qreal LayoutGap = 24;

// Height of a node box with `rows` port rows; must match PatchNode.qml.
qreal nodeHeight(int rows);

// Sort key for port names so channels list as FL, FR, FC, ... and monitor ports ("mon FL") after
// the regular ones; unknown names keep their relative order between the two groups.
int channelOrder(const QString &portName);

// Top-left position of every node that has ports.
QHash<quint32, QPointF> autoLayout(const GraphState &state);

} // namespace v9::pw
