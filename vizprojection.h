#ifndef VIZPROJECTION_H
#define VIZPROJECTION_H

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QObject>

#include <vector>

#include "projectionview.h"
#include "vizcoordinatesystem.h"
#include "vizmanipulationmanager.h"
#include "vizproperties.h"
#include "vizselectionmanager.h"

class VizProjection : public QObject
{
  Q_OBJECT
public:
  VizProjection(int horizontalDimensionIdx, int verticalDimensionIdx, QObject *parent = 0);
  QWidget *widget() {
    return m_view;
  }

  void projectScop(ClintScop *vscop);

  /*inline*/ VizProperties *vizProperties() const {
    return m_vizProperties;
  }

  /*inline*/ VizSelectionManager *selectionManager() const {
    return m_selectionManager;
  }

  /*inline*/ VizManipulationManager *manipulationManager() const {
    return m_manipulationManager;
  }

  void updateColumnHorizontalMinMax(VizCoordinateSystem *coordinateSystem, int minOffset, int maxOffset);
  void ensureFitsHorizontally(VizCoordinateSystem *coordinateSystem, int minimum, int maximum);
  void ensureFitsVertically(VizCoordinateSystem *coordinateSystem, int minimum, int maximum);

  int horizontalDimensionIdx() const {
    return m_horizontalDimensionIdx;
  }

  int verticalDimensionIdx() const {
    return m_verticalDimensionIdx;
  }

  enum class IsCsAction { Found, InsertPile, InsertCS };

  class IsCsResult {
  public:
    IsCsAction action() const {
      return m_action;
    }
    size_t pileIdx() const {
      return m_pile;
    }
    size_t coordinateSystemIdx() const {
      return m_coordinateSystem;
    }
    VizCoordinateSystem *coordinateSystem() const {
      return m_vcs;
    }

    void setFound(VizCoordinateSystem *cs) {
      if (m_action == IsCsAction::InsertPile) {
        m_coordinateSystem = 0;
      }
      m_action = IsCsAction::Found;
      m_vcs = cs;
    }

  private:
    size_t m_pile;   // in case InsertPile, insert before this index; if index >= size, insert after the last; in case InsertCS, index of the pile to insert to
    size_t m_coordinateSystem;
    IsCsAction m_action;
    VizCoordinateSystem *m_vcs = nullptr;

    friend class VizProjection;
  };

  VizProjection::IsCsResult isCoordinateSystem(QPointF point);
  VizCoordinateSystem *ensureCoordinateSystem(IsCsResult &csAt, int dimensionality);
  VizCoordinateSystem *createCoordinateSystem(int dimensionality);
  void deleteCoordinateSystem(VizCoordinateSystem *vcs);

  void paintProjection(QPainter *painter) {
    m_scene->render(painter);
  }

  QSize projectionSize() const {
    return m_scene->itemsBoundingRect().size().toSize();
  }

  std::pair<size_t, size_t> csIndices(VizCoordinateSystem *vcs) const;
  size_t pileCSNumber(size_t pileIndex) const {
    CLINT_ASSERT(pileIndex < m_coordinateSystems.size(), "Pile index out of range");
    return m_coordinateSystems.at(pileIndex).size();
  }
  size_t pileNumber() const {
    return m_coordinateSystems.size();
  }

  VizCoordinateSystem *insertPile(IsCsResult csAt, int dimensionality);
  VizCoordinateSystem *insertCs(IsCsResult csAt, int dimensionality);
  void updateOuterDependences();
  void updateInnerDependences();
  void updateInternalDependences();

  void setViewActive(bool active) {
    if (m_view)
      m_view->setActive(active);
  }

  bool isViewActive() const {
    return m_view && m_view->isActive();
  }

signals:
  void selected(int horizontal, int vertical);

public slots:
  void updateProjection();
  void selectProjection();

private:
  ProjectionView *m_view;
  QGraphicsScene *m_scene;
  VizProperties *m_vizProperties;

  // Outer index = column index;
  // Inner index = row index within the given column
  // Different columns may have different number of rows.
  std::vector<std::vector<VizCoordinateSystem *>> m_coordinateSystems;
  int m_horizontalDimensionIdx;
  int m_verticalDimensionIdx;

  VizSelectionManager *m_selectionManager;
  VizManipulationManager *m_manipulationManager;

  void appendCoordinateSystem(int dimensionality);
  void updateSceneLayout();
};

#endif // VIZPROJECTION_H
