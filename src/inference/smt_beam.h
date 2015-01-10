/*
 * smt_beam.h
 *
 *  Created on: Nov 27, 2014
 *      Author: veselin
 */

#ifndef INFERENCE_SMT_BEAM_H_
#define INFERENCE_SMT_BEAM_H_


#include "cvc4/cvc4.h"


class CVC4Call {
public:
  CVC4Call() : em_(), smt_(&em_) {
  }

  struct Indicator {
    Indicator(int v1, int v2, int s) : value1(v1), value2(v2), score(s) {
    }

    int value1;
    int value2;
    int score;
  };

  CVC4::Expr GetScore(CVC4::Expr v1, CVC4::Expr v2, std::vector<Indicator>& inds) {
    /*CVC4::Expr s = em_.mkConst(CVC4::Rational(0, 1));
    for (Indicator i : inds) {
      CVC4::Expr cond = em_.mkExpr(CVC4::kind::AND,
          em_.mkExpr(CVC4::kind::EQUAL, v1, em_.mkConst(CVC4::Rational(i.value1, 1))),
          em_.mkExpr(CVC4::kind::EQUAL, v2, em_.mkConst(CVC4::Rational(i.value2, 1))));
      s = cond.iteExpr(em_.mkConst(CVC4::Rational(i.score, 1)), s);
    }
    return s;*/
    /*
    CVC4::Expr zero = em_.mkConst(CVC4::Rational(0, 1));
    std::vector<CVC4::Expr> els;
    for (Indicator i : inds) {
      CVC4::Expr cond = em_.mkExpr(CVC4::kind::AND,
          em_.mkExpr(CVC4::kind::EQUAL, v1, GetVConst(i.value1)),
          em_.mkExpr(CVC4::kind::EQUAL, v2, GetVConst(i.value2)));
      els.push_back(cond.iteExpr(em_.mkConst(CVC4::Rational(i.score, 1)), zero));
    }
    if (els.empty()) return zero;
    else if (els.size() == 1) return els[0];
    return em_.mkExpr(CVC4::Kind::PLUS, els);
    */
    CVC4::Expr score = em_.mkVar("score", em_.integerType());
    for (Indicator i : inds) {
      CVC4::Expr cond = em_.mkExpr(CVC4::kind::AND,
          em_.mkExpr(CVC4::kind::EQUAL, v1, GetVConst(i.value1)),
          em_.mkExpr(CVC4::kind::EQUAL, v2, GetVConst(i.value2)));
      constraints_.push_back(cond.impExpr(em_.mkExpr(CVC4::kind::EQUAL, score, em_.mkConst(CVC4::Rational(i.score, 1)))));
    }
    return score;
  }

  void BoundIntegerExpr(CVC4::Expr e, int low, int high) {
    constraints_.push_back(em_.mkExpr(CVC4::kind::BITVECTOR_UGE, e, GetVConst(low)));
    constraints_.push_back(em_.mkExpr(CVC4::kind::BITVECTOR_ULE, e, GetVConst(high)));
  }

  CVC4::Expr GetVConst(unsigned value) {
    return em_.mkConst(CVC4::BitVector(8, value));
  }

  /*
  void BoundIntegerExpr(CVC4::Expr e, int low, int high) {
    constraints_.push_back(em_.mkExpr(CVC4::kind::GEQ, e, GetVConst(low)));
    constraints_.push_back(em_.mkExpr(CVC4::kind::LEQ, e, GetVConst(high)));
  }

  CVC4::Expr GetVConst(int value) {
    return em_.mkConst(CVC4::Rational(value, 1));
  }
*/
  void Go() {
    smt_.setOption("produce-models", true);

    printf("Go\n");

    // em_.mkConst(CVC4::BitVector(32, 5));

    v_type_ = em_.mkBitVectorType(8);  // em_.integerType();
    // v_type_ = em_.integerType();
    CVC4::Expr v1 = em_.mkVar("x", v_type_);
    CVC4::Expr v2 = em_.mkVar("y", v_type_);

    BoundIntegerExpr(v1, 0, 127);
    BoundIntegerExpr(v2, 0, 127);

    std::vector<Indicator> inds;
    for (size_t i = 0; i < 128; ++i) {
      for (size_t j = 0; j < 128; ++j) {
        inds.push_back(Indicator(i, j, i*j));
      }
    }
    /*inds.push_back(Indicator(0, 0, 4));
    inds.push_back(Indicator(0, 1, 3));
    inds.push_back(Indicator(1, 0, 3));
    inds.push_back(Indicator(1, 1, 8));*/
    CVC4::Expr score = GetScore(v1, v2, inds);

    CVC4::Expr min_score = em_.mkConst(CVC4::Rational(127*127, 1));
    constraints_.push_back(em_.mkExpr(CVC4::kind::GEQ, score, min_score));

    // constraints_.push_back(em_.mkExpr(CVC4::kind::LT, v1, v2));
    CVC4::Expr e = em_.mkExpr(CVC4::kind::AND, constraints_);
    LOG(INFO) << "Add";

    // LOG(INFO) << e;
    LOG(INFO) << "Start";

    CVC4::Result r = smt_.checkSat(e);


    LOG(INFO) << r;

    if (r.isSat() == CVC4::Result::SAT) {
      printf("SAT\n");
      LOG(INFO) << smt_.getValue(v1);
      LOG(INFO) << smt_.getValue(v2);
      CVC4::Statistics stats = smt_.getStatistics();
      for (auto it = stats.begin(); it != stats.end(); ++it) {
        LOG(INFO) << (*it).first << " " << (*it).second;
      }
    } else if (r.isSat() == CVC4::Result::UNSAT) {
      printf("UNSAT\n");
    } else {
      printf("? %d\n", r.isValid());
    }
  }

private:
  std::vector<CVC4::Expr> constraints_;

  CVC4::Type v_type_;
  CVC4::ExprManager em_;
  CVC4::SmtEngine smt_;
};


void Start() {
  CVC4Call c;
  c.Go();
}


#endif /* INFERENCE_SMT_BEAM_H_ */
