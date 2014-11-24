#ifndef CLINTSCOP_H
#define CLINTSCOP_H

#include <QObject>

#include "clintprogram.h"

#include <map>
#include <unordered_set>
#include <vector>

#include "transformation.h"
#include "transformer.h"

class ClintDependence;
class ClintStmt;
class ClintStmtOccurrence;

class ClintScop : public QObject {
  Q_OBJECT
public:
  typedef std::map<std::vector<int>, ClintStmt *> VizBetaMap;
  typedef std::multimap<std::pair<std::vector<int>, std::vector<int>>, ClintDependence *> ClintDependenceMap;
  typedef std::multimap<ClintStmtOccurrence *, ClintDependence *> ClintOccurrenceDeps;

  explicit ClintScop(osl_scop_p scop, ClintProgram *parent = nullptr);
  ~ClintScop();

  // Accessors
  ClintProgram *program() const {
    return program_;
  }

  const VizBetaMap &vizBetaMap() const {
    return m_vizBetaMap;
  }

  osl_relation_p fixedContext() const {
    return m_fixedContext;
  }

  std::unordered_set<ClintStmt *> statements() const;

  ClintStmt *statement(const std::vector<int> &beta) const {
    auto iterator = m_vizBetaMap.find(beta);
    if (iterator == std::end(m_vizBetaMap))
      return nullptr;
    return iterator->second;
  }

  void transformed(const Transformation &t) {
    TransformationGroup tg;
    tg.transformations.push_back(t);
    m_transformationSeq.groups.push_back(std::move(tg));
  }

  void transformed(const TransformationGroup &tg) {
    m_transformationSeq.groups.push_back(tg);
  }

  void executeTransformationSequence() {
    m_transformer->apply(m_scop_part, m_transformationSeq);
    osl_scop_print(stderr, m_scop_part);
    m_transformationSeq.groups.clear(); // Execute sequence and "forget" about it.
    // TODO: keep the original scop and transformation sequence; apply it before generating points.
  }

  ClintStmtOccurrence *occurrence(const std::vector<int> &beta) const;
  std::unordered_set<ClintDependence *> internalDependences(ClintStmtOccurrence *occurrence) const;
  void createDependences(osl_scop_p scop);
signals:

public slots:

private:

  osl_scop_p m_scop_part;
  ClintProgram *program_;
  osl_relation_p m_fixedContext;
//  std::vector<VizStatement *> statements_;
  // statements = unique values of m_vizBetaMap
  VizBetaMap m_vizBetaMap;
  ClintDependenceMap m_dependenceMap;
  ClintOccurrenceDeps m_internalDeps;

  TransformationSequence m_transformationSeq;
  Transformer *m_transformer;
};

#endif // CLINTSCOP_H
