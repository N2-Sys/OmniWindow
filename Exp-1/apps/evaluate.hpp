#ifndef EVALUATE_HPP
#define EVALUATE_HPP

#include <functional>
#include <memory>
#include <future>
#include <type_traits>

#include "Trace.h"
#include "Query.hpp"

#include "Distinct.hpp"
#include "Reduce.hpp"

struct VoidDKey {};

template <typename Key, typename DKey, typename RKey, typename RVal, typename TQryGen>
void evaluate(const char *tracePath, TQryGen qryGen, size_t distinctLen = 65536 * 32, size_t reduceLen = 65536) {

  size_t nEpoch = 100; double interval = 0.5; double dcTime = 20e-3;
  size_t nSlide = 5;

  double slide = interval / (double)nSlide;

  Trace trace = readTrace(tracePath);

  auto fResISW = std::async(std::launch::async, getResult<Key>,
      trace, nEpoch * nSlide, slide, 0,
      qryGen(
        [nSlide]() {
          return std::make_unique<SlidingBaselineDistinct<DKey>>(nSlide); },
        [nSlide]() {
          return std::make_unique<SlidingReduce<RKey, RVal>>(
            nSlide, std::make_unique<BaselineReduce<RKey, RVal>>()); }));

  auto fResTW1 = std::async(std::launch::async, getResult<Key>,
      trace, nEpoch, interval, dcTime,
      qryGen(
        [distinctLen] { return std::make_unique<DataPlaneDistinct<DKey>>(distinctLen * 4); },
        [reduceLen] { return std::make_unique<DataPlaneReduce<RKey, RVal>>(reduceLen * 4); }));

  auto fResTW2 = std::async(std::launch::async, getResult<Key>,
      trace, nEpoch, interval, 0,
      qryGen(
        [distinctLen]() { return std::make_unique<DataPlaneDistinct<DKey>>(distinctLen * 4); },
        [reduceLen]() { return std::make_unique<DataPlaneReduce<RKey, RVal>>(reduceLen * 4); }));

  auto fResOTW = std::async(std::launch::async, getResult<Key>,
      trace, nEpoch * nSlide, slide, 0,
      qryGen(
        [nSlide, distinctLen]() {
          return std::make_unique<FineGrainedLargeDistinct<DKey>>(
            nSlide, std::make_unique<DataPlaneDistinct<DKey>>(distinctLen)); },
        [nSlide, reduceLen]() {
          return std::make_unique<FineGrainedReduce<RKey, RVal>>(
            nSlide, std::make_unique<DataPlaneReduce<RKey, RVal>>(reduceLen)); }));

  auto fResITW = std::async(std::launch::async, getResult<Key>,
    trace, nEpoch, interval, 0,
    qryGen(
      []() { return std::make_unique<BaselineDistinct<DKey>>(); },
      []() { return std::make_unique<BaselineReduce<RKey, RVal>>(); }));

  auto fResOSW = std::async(std::launch::async, getResult<Key>,
      trace, nEpoch * nSlide, slide, 0,
      qryGen(
        [nSlide, distinctLen]() {
          return std::make_unique<FineGrainedLargeDistinct<DKey>>(
            nSlide, std::make_unique<DataPlaneDistinct<DKey>>(distinctLen)); },
        [nSlide, reduceLen]() {
          return std::make_unique<SlidingReduce<RKey, RVal>>(
            nSlide, std::make_unique<DataPlaneReduce<RKey, RVal>>(reduceLen)); }));

  auto mergedISW = collapseResultAll(fResISW.get());
  ResultInfo<Key>(mergedISW, collapseResultAll(fResTW1.get())).print("TW1");
  ResultInfo<Key>(mergedISW, collapseResultAll(fResTW2.get())).print("TW2");
  ResultInfo<Key>(mergedISW, collapseResultAll(fResOTW.get())).print("OTW");
  ResultInfo<Key>(mergedISW, collapseResultAll(fResITW.get())).print("ITW");
  ResultInfo<Key>(mergedISW, collapseResultAll(fResOSW.get())).print("OSW");
}

#endif
