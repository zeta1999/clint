#include "vizcoordinatesystem.h"
#include "vizmanipulationmanager.h"
#include "vizpoint.h"
#include "vizpolyhedron.h"
#include "vizproperties.h"
#include "vizprojection.h"
#include "vizselectionmanager.h"

#include <QtGui>
#include <QtWidgets>

VizManipulationManager::VizManipulationManager(QObject *parent) :
  QObject(parent) {
}

void VizManipulationManager::ensureTargetConsistency() {
  int sum = 0;
  sum += (m_polyhedron != nullptr);
  sum += (m_point != nullptr);
  sum += (m_coordinateSystem != nullptr);
  // Should not be possible unless we are in the multitouch environment.
  CLINT_ASSERT(sum == 1, "More than one active movable object manipulated at the same time");
}

void VizManipulationManager::polyhedronAboutToMove(VizPolyhedron *polyhedron) {
  CLINT_ASSERT(m_polyhedron == nullptr, "Active polyhedron is already being manipulated");
  m_polyhedron = polyhedron;
  ensureTargetConsistency();

  m_horzOffset = 0;
  m_vertOffset = 0;
  m_firstMovement = true;

  int hmax, vmax;
  polyhedron->coordinateSystem()->minMax(m_initCSHorizontalMin, hmax, m_initCSVerticalMin, vmax);
}

void VizManipulationManager::polyhedronMoving(VizPolyhedron *polyhedron, QPointF displacement) {
  CLINT_ASSERT(polyhedron == m_polyhedron, "Another polyhedron is being manipulated");

  if (m_firstMovement) {
    m_firstMovement = false;
    const std::unordered_set<VizPolyhedron *> &selectedPolyhedra =
        polyhedron->coordinateSystem()->projection()->selectionManager()->selectedPolyhedra();
    for (VizPolyhedron *vp : selectedPolyhedra) {
      vp->setOverrideSetPos(true);
    }
  }

  VizCoordinateSystem *cs = polyhedron->coordinateSystem();
  const VizProperties *properties = cs->projection()->vizProperties();
  double absoluteDistanceX = displacement.x() / properties->pointDistance();
  double absoluteDistanceY = -displacement.y() / properties->pointDistance(); // Inverted vertical axis
  int horzOffset = static_cast<int>(round(absoluteDistanceX));
  int vertOffset = static_cast<int>(round(absoluteDistanceY));
  if (horzOffset != m_horzOffset || vertOffset != m_vertOffset) {
    // Get selected objects.  (Should be in the same coordinate system.)
    const std::unordered_set<VizPolyhedron *> &selectedPolyhedra =
        cs->projection()->selectionManager()->selectedPolyhedra();
    CLINT_ASSERT(std::find(std::begin(selectedPolyhedra), std::end(selectedPolyhedra), polyhedron) != std::end(selectedPolyhedra),
                 "The active polyhedra is not selected");
    int phHmin = INT_MAX, phHmax = INT_MIN, phVmin = INT_MAX, phVmax = INT_MIN;
    for (VizPolyhedron *ph : selectedPolyhedra) {
      phHmin = std::min(phHmin, ph->localHorizontalMin());
      phHmax = std::max(phHmax, ph->localHorizontalMax());
      phVmin = std::min(phVmin, ph->localVerticalMin());
      phVmax = std::max(phVmax, ph->localVerticalMax());
    }

    if (horzOffset != m_horzOffset) {
      int hmin = phHmin + horzOffset;
      int hmax = phHmax + horzOffset;
      cs->projection()->ensureFitsHorizontally(cs, hmin, hmax);
      emit intentionMoveHorizontally(horzOffset - m_horzOffset);
      m_horzOffset = horzOffset;
    }

    if (vertOffset != m_vertOffset) {
      int vmin = phVmin + vertOffset;
      int vmax = phVmax + vertOffset;
      cs->projection()->ensureFitsVertically(cs, vmin, vmax);
      emit intentionMoveVertically(vertOffset - m_vertOffset);
      m_vertOffset = vertOffset;
    }
  }
}

void VizManipulationManager::polyhedronHasMoved(VizPolyhedron *polyhedron) {
  CLINT_ASSERT(m_polyhedron == polyhedron, "Signaled end of polyhedron movement that was never initiated");
  m_polyhedron = nullptr;
  m_firstMovement = false;
  TransformationGroup group;
  const std::unordered_set<VizPolyhedron *> &selectedPolyhedra =
      polyhedron->coordinateSystem()->projection()->selectionManager()->selectedPolyhedra();
  for (VizPolyhedron *vp : selectedPolyhedra) {
    vp->setOverrideSetPos(false);
  }
  if (m_horzOffset != 0 || m_vertOffset != 0) {
    // TODO: move this code to transformation manager
    // we need transformation manager to keep three different views in sync
    CLINT_ASSERT(std::find(std::begin(selectedPolyhedra), std::end(selectedPolyhedra), polyhedron) != std::end(selectedPolyhedra),
                 "The active polyhedra is not selected");

    // TODO: vertical should be ignored (and movement forbidden) for 1D coordinate systems
    for (VizPolyhedron *vp : selectedPolyhedra) {
      const std::vector<int> beta = vp->occurrence()->betaVector();
      int horzDepth = vp->coordinateSystem()->horizontalDimensionIdx() + 1;
      int horzAmount = m_horzOffset;
      int vertDepth = vp->coordinateSystem()->verticalDimensionIdx() + 1;
      int vertAmount = m_vertOffset;
      if (m_horzOffset != 0) {
        Transformation transformation = Transformation::consantShift(beta, horzDepth, -horzAmount);
        group.transformations.push_back(transformation);
      }
      if (m_vertOffset != 0) {
        Transformation transformation = Transformation::consantShift(beta, vertDepth, -vertAmount);
        group.transformations.push_back(transformation);
      }

      int hmin, hmax, vmin, vmax;
      vp->coordinateSystem()->minMax(hmin, hmax, vmin, vmax);
      // If the coordinate system was extended in the negative direction (minimum decreased)
      // during this transformation AND the polyhedron is positioned at the lower extrema of
      // the coordiante system, update its position.  Fixes the problem with big extensions of
      // the CS if a polyhedron ends up outside coordinate system.
      const bool fixHorizontal = hmin < m_initCSHorizontalMin &&
                                 hmin >= (vp->localHorizontalMin() + m_horzOffset);
      const bool fixVertical   = vmin < m_initCSVerticalMin &&
                                 vmin >= (vp->localVerticalMin() + m_vertOffset);
      if (fixHorizontal && fixVertical) {
        vp->coordinateSystem()->setPolyhedronCoordinates(vp, hmin, vmin);
      } else if (fixHorizontal) {
        vp->coordinateSystem()->setPolyhedronCoordinates(vp, hmin, INT_MAX, false, true);
      } else if (fixVertical) {
        vp->coordinateSystem()->setPolyhedronCoordinates(vp, INT_MAX, vmin, true, false);
      }
      CLINT_ASSERT(vp->scop() == polyhedron->scop(), "All statement occurrences in the transformation group must be in the same scop");
    }

    if (m_horzOffset != 0)
      emit movedHorizontally(m_horzOffset);
    if (m_vertOffset != 0)
      emit movedVertically(m_vertOffset);
  } else {
    for (VizPolyhedron *vp : selectedPolyhedra) {
      vp->coordinateSystem()->polyhedronUpdated(vp);
    }
  }

  if (!group.transformations.empty()) {
    polyhedron->scop()->transform(group);
    polyhedron->scop()->executeTransformationSequence();
  }
}


void VizManipulationManager::polyhedronAboutToDetach(VizPolyhedron *polyhedron) {
  CLINT_ASSERT(m_polyhedron == nullptr, "Active polyhedron is already being manipulated");
  m_polyhedron = polyhedron;
  ensureTargetConsistency();

  CLINT_ASSERT(m_polyhedron->coordinateSystem()->coordinateSystemRect().contains(polyhedron->pos()),
               "Coordinate system rectangle does not contain the polyhedron");
  m_detached = false;

  QPointF position = polyhedron->coordinateSystem()->mapToParent(polyhedron->pos());
  VizProjection::IsCsResult r = polyhedron->coordinateSystem()->projection()->isCoordinateSystem(position);
  CLINT_ASSERT(r.action() == VizProjection::IsCsAction::Found && r.coordinateSystem() == polyhedron->coordinateSystem(),
               "Polyhedron position is not found in the coordinate system it actually belongs to");
}

void VizManipulationManager::polyhedronDetaching(QPointF position) {
  QRectF mainRect = m_polyhedron->coordinateSystem()->coordinateSystemRect();
  if (!mainRect.contains(position)) {
    m_detached = true;
  }
}

void VizManipulationManager::rearrangePiles2D(std::vector<int> &createdBeta, bool pileDeleted, TransformationGroup &group, int pileIdx, size_t pileNb) {
  CLINT_ASSERT(createdBeta.size() >= 3, "Dimensionality mismatch");
  std::vector<int> pileBeta(std::begin(createdBeta), std::end(createdBeta) - 2);
  if (pileDeleted) {
    if (pileIdx == pileBeta.back() || pileIdx == pileBeta.back() + 1) {
      // Do nothing (cases DN1, DN2);
    } else {
      // Put before the given pile.
      group.transformations.push_back(Transformation::putBefore(pileBeta, pileIdx, pileNb));
    }
  } else {
    if (pileIdx == pileBeta.back()) {
      // Do nothing (case KN2);
    } else if (pileIdx > pileBeta.back()) {
      // Put before, account for pile not being deleted.
      group.transformations.push_back(Transformation::putBefore(pileBeta, pileIdx + 1, pileNb));
    } else {
      // Put before the given pile.
      group.transformations.push_back(Transformation::putBefore(pileBeta, pileIdx, pileNb));
    }
  }
}

void VizManipulationManager::rearrangeCSs2D(int coordinateSystemIdx, bool csDeleted, std::vector<int> &createdBeta, size_t pileSize, TransformationGroup &group) {
        // By analogy to insert pile.
  CLINT_ASSERT(createdBeta.size() >= 2, "Dimensionality mismatch");
  std::vector<int> csBeta(std::begin(createdBeta), std::end(createdBeta) - 1);
  if (csDeleted) {
    if (coordinateSystemIdx == csBeta.back() || coordinateSystemIdx == csBeta.back() + 1) {
      // Do nothing.
    } else {
      group.transformations.push_back(Transformation::putBefore(csBeta, coordinateSystemIdx, pileSize));
    }
  } else {
    if (coordinateSystemIdx == csBeta.back()) {
      // Do nothing.
    } else if (coordinateSystemIdx > csBeta.back()) {
  // FIXME: hack
  if (coordinateSystemIdx + 1 > pileSize)
    coordinateSystemIdx = pileSize - 1;
      group.transformations.push_back(Transformation::putBefore(csBeta, coordinateSystemIdx + 1, pileSize));
    } else {
      group.transformations.push_back(Transformation::putBefore(csBeta, coordinateSystemIdx, pileSize));
    }
  }
}

void VizManipulationManager::polyhedronHasDetached(VizPolyhedron *polyhedron) {
  CLINT_ASSERT(m_polyhedron == polyhedron, "Signaled end of polyhedron movement that was never initiated");
  m_polyhedron = nullptr;
  TransformationGroup group;

  const std::unordered_set<VizPolyhedron *> &selectedPolyhedra =
      polyhedron->coordinateSystem()->projection()->selectionManager()->selectedPolyhedra();
  CLINT_ASSERT(std::find(std::begin(selectedPolyhedra), std::end(selectedPolyhedra), polyhedron) != std::end(selectedPolyhedra),
               "The active polyhedra is not selected");
  if (m_detached) {
    QPointF position = polyhedron->coordinateSystem()->mapToParent(polyhedron->pos());
    VizProjection::IsCsResult r = polyhedron->coordinateSystem()->projection()->isCoordinateSystem(position);
    int dimensionality = -1;
    for (VizPolyhedron *vp : selectedPolyhedra) {
      dimensionality = std::max(dimensionality, vp->occurrence()->dimensionality());
    }
    VizCoordinateSystem *cs = polyhedron->coordinateSystem()->projection()->ensureCoordinateSystem(r, dimensionality);
    int hmin = INT_MAX, hmax = INT_MIN, vmin = INT_MAX, vmax = INT_MIN; // TODO: frequent functionality, should be extracted?
    // FIXME: all these assumes only all polyhedra belong to the same coordiante system.?
    for (VizPolyhedron *vp : selectedPolyhedra) {
      VizCoordinateSystem *oldCS = vp->coordinateSystem();
      size_t oldPileIdx, oldCsIdx;
      // FIXME: works for normalized scops only
      std::tie(oldPileIdx, oldCsIdx) = oldCS->projection()->csIndices(oldCS);
      bool oneDimensional = vp->occurrence()->dimensionality() < oldCS->verticalDimensionIdx();
      bool zeroDimensional = vp->occurrence()->dimensionality() < oldCS->horizontalDimensionIdx();

      bool movedToAnotherCS = oldCS != cs; // split is needed if the polygon was moved to a different CS

      if (!movedToAnotherCS) {
        vp->coordinateSystem()->resetPolyhedronPos(vp);
        continue;
      }


      bool csDeleted = false;
      bool pileDeleted = false;

      cs->reparentPolyhedron(vp);
      if (r.action() != VizProjection::IsCsAction::Found) {
        cs->resetPolyhedronPos(polyhedron);
      }
      // TODO: otherwise, shift it to the position it ended up graphically (reparent does it visually, but we must create a shift Transformation)
      hmin = std::min(hmin, vp->localHorizontalMin());
      hmax = std::max(hmax, vp->localHorizontalMax());
      vmin = std::min(vmin, vp->localVerticalMin());
      vmax = std::max(vmax, vp->localVerticalMax());
      if (oldCS->isEmpty()) {
        csDeleted = true;
        if (oldCS->projection()->pileCSNumber(oldPileIdx) == 1) {
          pileDeleted = true;
        }

        oldCS->projection()->deleteCoordinateSystem(oldCS);
      }

      CLINT_ASSERT(oneDimensional ? csDeleted : true, "In 1D cases, CS should be deleted");
      CLINT_ASSERT(zeroDimensional ? pileDeleted : true, "In 0D cases, pile should be deleted");

      size_t pileNb = cs->projection()->pileNumber();

      // Constructing transformation group.
      const std::vector<int> &beta = vp->occurrence()->betaVector();
      std::vector<int> createdBeta(beta);
      bool splitHorizontal = false, splitVertical = false;

      bool actionWithinPile = (r.action() == VizProjection::IsCsAction::Found ||
                               r.action() == VizProjection::IsCsAction::InsertCS) && r.pileIdx() == oldPileIdx;
      // Splitting the occurrence out.
      if (!csDeleted) { // if coordinate system was deleted, no inner split needed
        size_t csSize = oldCS->countPolyhedra() + 1; // it was already removed from CS
        std::vector<int> splitBeta(beta);
        splitBeta.push_back(424242);
        qDebug() << "inner split" << csSize << QVector<int>::fromStdVector(splitBeta);
        rearrangeCSs2D(csSize, true, splitBeta, csSize, group);
        CLINT_ASSERT(csSize >= 2, "Split requested where it should not happen");
        splitBeta.erase(std::end(splitBeta) - 1);
        splitBeta.back() = csSize - 2;
        group.transformations.push_back(Transformation::splitAfter(splitBeta));

        splitVertical = true;
      }
      if (!pileDeleted && !actionWithinPile) { // if pile was deleted or fusion is done within a pile, no outer split needed
        size_t pileSize = cs->projection()->pileCSNumber(oldPileIdx) + (csDeleted || !actionWithinPile ? 1 : 0); // if a cs was deleted, still account for it.
        std::vector<int> splitBeta(beta);
        // If inner split took place, the beta is changed.
        if (splitVertical) {
          splitBeta[splitBeta.size() - 2]++;
          splitBeta[splitBeta.size() - 1] = 0;
        }
        if (oneDimensional) {
          splitBeta.push_back(424242);
        }
        qDebug() << "outer split" << pileSize << QVector<int>::fromStdVector(splitBeta);
        rearrangeCSs2D(pileSize, true, splitBeta, pileSize, group);
        CLINT_ASSERT(pileSize >= 2, "Split requested where it should not happen");
        CLINT_ASSERT(splitBeta.size() >= 2, "Dimensionality mismatch");
        splitBeta.erase(std::end(splitBeta) - 1);
        splitBeta.back() = pileSize - 2;
        group.transformations.push_back(Transformation::splitAfter(splitBeta));

        splitHorizontal = true;
      }

      if (vp->coordinateSystem()->isVerticalAxisVisible() && vp->coordinateSystem()->isHorizontalAxisVisible()) {
        CLINT_ASSERT(createdBeta.size() >= 3, "Dimensionality mismatch");
        if (splitHorizontal) {
          qDebug() << "sh";
          createdBeta[createdBeta.size() - 1] = 0;
          createdBeta[createdBeta.size() - 2] = 0;
          createdBeta[createdBeta.size() - 3] += 1;
        } else if (splitVertical) {
          qDebug() << "sv";
          createdBeta[createdBeta.size() - 1] = 0;
          createdBeta[createdBeta.size() - 2] += 1;
        }
      } else if (vp->coordinateSystem()->isHorizontalAxisVisible()) {
        CLINT_ASSERT(createdBeta.size() >= 2, "Dimensionality mismatch");
        if (splitHorizontal) {
          createdBeta[createdBeta.size() - 1] = 0;
          createdBeta[createdBeta.size() - 2] += 1;
        }
      } else if (!vp->coordinateSystem()->isVerticalAxisVisible() && !vp->coordinateSystem()->isHorizontalAxisVisible()) {
        // Do nothing
      } else {
        CLINT_UNREACHABLE;
      }

      qDebug() << "beta update successful" << QVector<int>::fromStdVector(createdBeta);

      size_t targetPileIdx, targetCSIdx;
      std::tie(targetPileIdx, targetCSIdx) = cs->projection()->csIndices(cs);

      qDebug() << "targeting pile" << targetPileIdx << "cs" << targetCSIdx;

      if (r.action() == VizProjection::IsCsAction::Found) {
        // Fusion required.
        std::vector<int> fuseBeta = cs->betaPrefix(); // if found, it contains at least one polygon (other than vp) with correct beta.
        CLINT_ASSERT(fuseBeta[fuseBeta.size() - 1] == r.coordinateSystemIdx(), "Beta consistency violated");
        CLINT_ASSERT(fuseBeta[fuseBeta.size() - 2] == r.pileIdx(), "Beta consistency violated");

        size_t pileIdx = r.pileIdx();
        if (pileDeleted && oldPileIdx < pileIdx) {
          pileIdx--;
          fuseBeta[fuseBeta.size() - 2]--;
        }

        // TODO: same for CS?
        size_t csIdx = r.coordinateSystemIdx();
        if (csDeleted && actionWithinPile && oldCsIdx < csIdx) {
          csIdx--;
          fuseBeta[fuseBeta.size() - 1]--;
        }

        // First, rearrange and fuse on the outer level.  For fusion's sake, put the statement after the pile to fuse with.
//        rearrangePiles2D(createdBeta, true, group, pileIdx + 1 + (pileDeleted ? 0 : 1), pileNb + (pileDeleted ? 0 : 1));
        // Fuse if not within pile transformation.
        if (!actionWithinPile) {
          if (oneDimensional)
            createdBeta.push_back(424242);
          qDebug() << "outer fuse" << pileNb + (actionWithinPile ? 0 : 1) << pileIdx << QVector<int>::fromStdVector(fuseBeta) << QVector<int>::fromStdVector(createdBeta);
          rearrangePiles2D(createdBeta, false, group, pileIdx + 1, pileNb + (actionWithinPile ? 0 : 1));
          std::vector<int> outerFuseBeta(std::begin(fuseBeta), std::end(fuseBeta) - 1);
          group.transformations.push_back(Transformation::fuseNext(outerFuseBeta));
        }

        // Finally, rearrange and fuse on the inner level.
        size_t pileSize = cs->projection()->pileCSNumber(pileIdx) + (csDeleted && actionWithinPile ? 0 : 1); // extra CS created and is not visually present
        if (!actionWithinPile) { // If outer fuse took place, this occurence is the last in pile
          createdBeta[createdBeta.size() - 3] = pileIdx;
          createdBeta[createdBeta.size() - 2] = pileSize - 1;
          createdBeta[createdBeta.size() - 1] = 0;
        }
        qDebug() << "inner fuse" << pileSize << csIdx << QVector<int>::fromStdVector(createdBeta);
        if (!oneDimensional) {
          rearrangeCSs2D(csIdx + 1, csDeleted, createdBeta, pileSize, group);
          group.transformations.push_back(Transformation::fuseNext(fuseBeta));
        }

      } else if (r.action() == VizProjection::IsCsAction::InsertPile) {
        if (oneDimensional)
          createdBeta.push_back(424242);
        if (zeroDimensional) {
          createdBeta.push_back(424242);
          createdBeta.push_back(424242);
        }
        qDebug() << "insert pile";
        rearrangePiles2D(createdBeta, pileDeleted, group, r.pileIdx(), pileNb);
      } else if (r.action() == VizProjection::IsCsAction::InsertCS) {
//        if (oldPileIdx == r.pileIdx()) { // same pile -- reorder only
        if (actionWithinPile) {
          size_t pileSize = cs->projection()->pileCSNumber(r.pileIdx());
          if (oneDimensional)
            createdBeta.push_back(424242);
          qDebug() << "insert cs" << pileSize << QVector<int>::fromStdVector(createdBeta);
          rearrangeCSs2D(r.coordinateSystemIdx(), csDeleted, createdBeta, pileSize, group);
        } else { // different piles -- fusion required
          qDebug() << "insert cs" << QVector<int>::fromStdVector(createdBeta);

          size_t pileIdx = r.pileIdx();
          if (pileDeleted && oldPileIdx < pileIdx) {
            pileIdx--;
          }

          // TODO: same for CS?
          size_t csIdx = r.coordinateSystemIdx();
          if (csDeleted && actionWithinPile && oldCsIdx < csIdx) {
            csIdx--;
          }
          std::vector<int> fuseBeta { (int) pileIdx, (int) csIdx }; // <0,1> projection; otherwise first part of the beta-prefix ("projection" prefix) needed

          // First, rearrange and fuse on the outer level.  For fusion's sake, put the statement after the pile to fuse with.
//          rearrangePiles2D(createdBeta, true, group, pileIdx + 1 + (pileDeleted ? 0 : 1), pileNb + (pileDeleted ? 0 : 1));
          if (!actionWithinPile) {
            if (oneDimensional)
              createdBeta.push_back(424242);
            rearrangePiles2D(createdBeta, pileDeleted, group, pileIdx + 1, pileNb + (actionWithinPile ? 0 : 1));
            qDebug() << "outer fuse" << pileNb + (pileDeleted ? 0 : 1) << pileIdx << QVector<int>::fromStdVector(fuseBeta);
            std::vector<int> outerFuseBeta(std::begin(fuseBeta), std::end(fuseBeta) - 1);
            group.transformations.push_back(Transformation::fuseNext(outerFuseBeta));
          }

          // Finally, rearrange and fuse on the inner level.
          size_t pileSize = cs->projection()->pileCSNumber(pileIdx); // extra CS created is actually present
          if (!actionWithinPile) { // If outer fuse took place, this occurence is the last in pile
            createdBeta[createdBeta.size() - 3] = pileIdx;
            createdBeta[createdBeta.size() - 2] = pileSize - 1;
            createdBeta[createdBeta.size() - 1] = 0;
          }
          qDebug() << "inner fuse" << pileSize << csIdx << QVector<int>::fromStdVector(createdBeta) << oneDimensional;
          if (!oneDimensional)
            rearrangeCSs2D(csIdx, csDeleted, createdBeta, pileSize, group); // no (csIdx + 1) since to fusion required; putting before.

        }
      } else {
        CLINT_UNREACHABLE;
      }

    }

    Transformer *transformer = new ClayScriptGenerator(std::cerr);
    transformer->apply(nullptr, group);

    // TODO: provide functionality for both simultaneously with a single repaint (setMinMax?)
    cs->projection()->ensureFitsHorizontally(cs, hmin, hmax);
    cs->projection()->ensureFitsVertically(cs, vmin, vmax);
  } else {
    for (VizPolyhedron *vp : selectedPolyhedra) {
      vp->coordinateSystem()->resetPolyhedronPos(vp);
    }
  }

  if (!group.transformations.empty()) {
    ClayBetaMapper *mapper = new ClayBetaMapper(polyhedron->scop());
    mapper->apply(nullptr, group);
    bool happy = true;
    std::map<std::vector<int>, std::vector<int>> mapping;
    for (ClintStmt *stmt : polyhedron->scop()->statements()) {
      for (ClintStmtOccurrence *occurrence : stmt->occurences()) {
        int result;
        std::vector<int> beta = occurrence->betaVector();
        std::vector<int> updatedBeta;
        std::tie(updatedBeta, result) = mapper->map(occurrence->betaVector());

        qDebug() << result << QVector<int>::fromStdVector(beta) << "->" << QVector<int>::fromStdVector(updatedBeta);
        if (result == ClayBetaMapper::SUCCESS &&
            beta != updatedBeta) {
          occurrence->resetBetaVector(updatedBeta);
          mapping[beta] = updatedBeta;
        }
        happy = happy && result == ClayBetaMapper::SUCCESS;
      }
    }
    CLINT_ASSERT(happy, "Beta mapping failed");

    polyhedron->scop()->updateBetas(mapping);

    polyhedron->scop()->transform(group);
    polyhedron->scop()->executeTransformationSequence();
  }
}

