//
// Created by xzl on 1/11/18.
//

#include "PValues.h"
#include "core/Transforms.h"

//PCollection* PCollection::apply1(Pipeline *pipeline, PTransform* t) {
PCollection* PCollection::apply1(PTransform* t) {

  t->inputs.push_back(dynamic_cast<PValue *>(this));
  this->consumer = t;

  // only one output from @t is enough
  if (!t->outputs.size()) {
    PCollection * p = new PCollection(this->_pipeline); // yes we leak memory...
    t->outputs.push_back(dynamic_cast<PValue *>(p));
    p->producer = t;
  }

  return dynamic_cast<PCollection*>(t->outputs[0]);
}

//vector<PCollection*> PCollection::apply2(Pipeline *pipeline, PTransform* t) {
vector<PCollection*> PCollection::apply2(PTransform* t) {
  PCollection * out1 = new PCollection(this->_pipeline); // yes we leak memory...
  PCollection * out2 = new PCollection(this->_pipeline); // yes we leak memory...

  t->inputs.push_back(dynamic_cast<PValue *>(this));
  this->consumer = t;

  t->outputs.push_back(dynamic_cast<PValue *>(out1));
  t->outputs.push_back(dynamic_cast<PValue *>(out2));
  out1->producer = t;
  out2->producer = t;

  return vector<PCollection*>({out1, out2});
}

PCollection* PBegin::apply1(PTransform* t) {
  PCollection * p = new PCollection(this->_pipeline); // yes we leak memory...

  t->inputs.push_back(dynamic_cast<PValue *>(this));
  this->consumer = t;

  t->outputs.push_back(dynamic_cast<PValue *>(p));
  p->producer = t;

  return p;
}

vector<PCollection*> PBegin::apply2(PTransform* t) {
  PCollection * out1 = new PCollection(this->_pipeline); // yes we leak memory...
  PCollection * out2 = new PCollection(this->_pipeline); // yes we leak memory...

  t->inputs.push_back(dynamic_cast<PValue *>(this));
  this->consumer = t;

  t->outputs.push_back(dynamic_cast<PValue *>(out1));
  t->outputs.push_back(dynamic_cast<PValue *>(out2));
  out1->producer = t;
  out2->producer = t;

  return vector<PCollection*>({out1, out2});
}

vector<PCollection*> PBegin::applyK(PTransform* t, int k) {
  vector<PCollection *> outs;

  for (auto i = 0; i < k; i++)
    outs.emplace_back(new PCollection(this->_pipeline)); // yes we leak memory...

  t->inputs.push_back(dynamic_cast<PValue *>(this));
  this->consumer = t;

  for (auto & o : outs) {
    t->outputs.push_back(dynamic_cast<PValue *>(o));
    o->producer = t;
  }
  return outs;
}