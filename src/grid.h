#ifndef GRID_H_
#define GRID_H_

#include <array>
#include <cmath>
#include <iostream>  // TODO remove
#include <limits>
#include <vector>
#include "inline_vector.h"

namespace bdm {

using std::array;
using std::vector;
using std::fmod;

class Grid {
 public:
  struct Box {
    Grid* grid_;
    uint64_t start_ = 0;
    uint16_t length_ = 0;

    Box(Grid* grid) : grid_(grid) {}

    bool IsEmpty() const { return length_ == 0; }

    void AddCell(size_t cell_id) {
      if (IsEmpty()) {
        start_ = cell_id;
      } else {
        // add to front
        grid_->successors_[cell_id] = start_;
        start_ = cell_id;
      }
      length_++;
    }

    // TODO remove cell

    struct Iterator {
      Iterator(Grid* grid, const Box* box)
          : grid_(grid),
            current_value_(box->start_),
            countdown_(box->length_) {}

      bool IsAtEnd() { return countdown_ <= 0; }

      Iterator operator++() {
        countdown_--;
        current_value_ = grid_->successors_[current_value_];
        return *this;
      }

      size_t operator*() const { return current_value_; }

      Grid* grid_;
      size_t current_value_;
      int countdown_;
    };

    Iterator begin() const { return Iterator(grid_, this); }
  };

  struct NeighborIterator {
    NeighborIterator(InlineVector<const Box*, 27>* neighbor_boxes)
        : neighbor_boxes_(neighbor_boxes),
          box_iterator_((*neighbor_boxes_)[0]->begin()) {
      // if first box is empty
      if ((*neighbor_boxes_)[0]->IsEmpty()) {
        ForwardToNonEmptyBox();
      }
    }

    bool IsAtEnd() const { return is_end_; }

    size_t operator*() const { return *box_iterator_; }

    /// version where empty neighbors in neighbor_boxes_ are allowed
    NeighborIterator& operator++() {
      ++box_iterator_;
      // if iterator of current box has come to an end, continue with next box
      if (box_iterator_.IsAtEnd()) {
        return ForwardToNonEmptyBox();
      }
      return *this;
    }

   private:
    InlineVector<const Box*, 27>* neighbor_boxes_;
    Box::Iterator box_iterator_;
    uint16_t box_idx_ = 0;
    bool is_end_ = false;

    /// Forwards the iterator to the next non empty box and returns itself
    /// If there are no non empty boxes is_end_ is set to true
    NeighborIterator& ForwardToNonEmptyBox() {
      // increment box id until non empty box has been found
      while (++box_idx_ < neighbor_boxes_->size()) {
        // box is empty -> continue
        if ((*neighbor_boxes_)[box_idx_]->IsEmpty()) {
          continue;
        }
        // no empty box has been found
        box_iterator_ = (*neighbor_boxes_)[box_idx_]->begin();
        return *this;
      }
      // all remaining boxes have been empty; reached end
      is_end_ = true;
      return *this;
    }
  };

  Grid(vector<array<double, 3>>& positions, uint32_t box_length,
       const std::array<double, 3> max_value)
      : positions_(positions), box_lenght_(box_length) {
    // TODO determine max_value based on given positions
    // FIXME for simplicity assumes no negative positions
    num_boxes_axis_[0] = static_cast<size_t>(max_value[0]) / box_length;
    num_boxes_axis_[1] = static_cast<size_t>(max_value[1]) / box_length;
    num_boxes_axis_[2] = static_cast<size_t>(max_value[2]) / box_length;

    num_boxes_xy_ = num_boxes_axis_[0] * num_boxes_axis_[1];
    auto total_num_boxes = num_boxes_xy_ * num_boxes_axis_[2];

    std::cout << "num_boxes x " << num_boxes_axis_[0] << std::endl;
    std::cout << "num_boxes y " << num_boxes_axis_[1] << std::endl;
    std::cout << "num_boxes z " << num_boxes_axis_[2] << std::endl;
    std::cout << "num_boxes xy " << num_boxes_xy_ << std::endl;

    boxes_.resize(total_num_boxes, Box(this));

    // initialize successors_;
    successors_.resize(positions_.size());

    // assign simulation objects to boxes
    for (size_t cell_id = 0; cell_id < positions_.size(); cell_id++) {
      auto& position = positions_[cell_id];
      auto box = GetBoxPointer(GetBoxIndex(position));
      box->AddCell(cell_id);
    }
  }

  // TODO pass lambda as parameter
  void ForEachNeighbor() {
    vector<size_t> sum(positions_.size());
#pragma omp parallel for
    for (size_t i = 0; i < positions_.size(); i++) {
      auto& position = positions_[i];
      auto idx = GetBoxIndex(position);

      InlineVector<const Box*, 27> neighbor_boxes;
      GetMooreBoxes(&neighbor_boxes, idx);

      // std::cout << "Neighbors of " << i << ": ";

      NeighborIterator it(&neighbor_boxes);
      while (!it.IsAtEnd()) {
        // volatile auto current_val = *it;

        // do something with it
        sum[i] += *it;
        // std::cout << *it << ", ";

        ++it;
      }

      // std::cout << std::endl;
    }
    std::cout << "cell id sum " << sum[4] << std::endl;
  }

 private:
  vector<Box> boxes_;
  vector<array<double, 3>>& positions_;
  /// length of a Box
  uint32_t box_lenght_;
  /// stores the number of boxes for each axis
  array<uint32_t, 3> num_boxes_axis_;
  /// number of boxes in the xy plane (=num_boxes_axis_[0] * num_boxes_axis_[1])
  size_t num_boxes_xy_;
  /// Implements linked list - array index = key, value: next element
  vector<size_t> successors_;

  void GetMooreBoxes(InlineVector<const Box*, 27>* neighbor_boxes,
                     size_t box_idx) {
    neighbor_boxes->push_back(GetBoxPointer(box_idx));

    // TODO add enumeration to select between different modes
    // Adjacent 6 (top, down, left, right, front and back)
    neighbor_boxes->push_back(GetBoxPointer(box_idx - num_boxes_xy_));
    neighbor_boxes->push_back(GetBoxPointer(box_idx + num_boxes_xy_));
    neighbor_boxes->push_back(GetBoxPointer(box_idx - num_boxes_axis_[1]));
    neighbor_boxes->push_back(GetBoxPointer(box_idx + num_boxes_axis_[1]));
    neighbor_boxes->push_back(GetBoxPointer(box_idx - 1));
    neighbor_boxes->push_back(GetBoxPointer(box_idx + 1));

    // Adjacent 12
    neighbor_boxes->push_back(
        GetBoxPointer(box_idx - num_boxes_xy_ - num_boxes_axis_[1]));
    neighbor_boxes->push_back(GetBoxPointer(box_idx - num_boxes_xy_ - 1));
    neighbor_boxes->push_back(GetBoxPointer(box_idx - num_boxes_axis_[1] - 1));
    neighbor_boxes->push_back(
        GetBoxPointer(box_idx + num_boxes_xy_ - num_boxes_axis_[1]));
    neighbor_boxes->push_back(GetBoxPointer(box_idx + num_boxes_xy_ - 1));
    neighbor_boxes->push_back(GetBoxPointer(box_idx + num_boxes_axis_[1] - 1));
    neighbor_boxes->push_back(
        GetBoxPointer(box_idx - num_boxes_xy_ + num_boxes_axis_[1]));
    neighbor_boxes->push_back(GetBoxPointer(box_idx - num_boxes_xy_ + 1));
    neighbor_boxes->push_back(GetBoxPointer(box_idx - num_boxes_axis_[1] + 1));
    neighbor_boxes->push_back(
        GetBoxPointer(box_idx + num_boxes_xy_ + num_boxes_axis_[1]));
    neighbor_boxes->push_back(GetBoxPointer(box_idx + num_boxes_xy_ + 1));
    neighbor_boxes->push_back(GetBoxPointer(box_idx + num_boxes_axis_[1] + 1));

    // Adjacent 8
    neighbor_boxes->push_back(
        GetBoxPointer(box_idx - num_boxes_xy_ - num_boxes_axis_[1] - 1));
    neighbor_boxes->push_back(
        GetBoxPointer(box_idx - num_boxes_xy_ - num_boxes_axis_[1] + 1));
    neighbor_boxes->push_back(
        GetBoxPointer(box_idx - num_boxes_xy_ + num_boxes_axis_[1] - 1));
    neighbor_boxes->push_back(
        GetBoxPointer(box_idx - num_boxes_xy_ + num_boxes_axis_[1] + 1));
    neighbor_boxes->push_back(
        GetBoxPointer(box_idx + num_boxes_xy_ - num_boxes_axis_[1] - 1));
    neighbor_boxes->push_back(
        GetBoxPointer(box_idx + num_boxes_xy_ + num_boxes_axis_[1] + 1));
    neighbor_boxes->push_back(
        GetBoxPointer(box_idx + num_boxes_xy_ + num_boxes_axis_[1] - 1));
    neighbor_boxes->push_back(
        GetBoxPointer(box_idx + num_boxes_xy_ + num_boxes_axis_[1] + 1));
  }

  const Box* GetBoxPointer(size_t index) const { return &(boxes_[index]); }

  Box* GetBoxPointer(size_t index) { return &(boxes_[index]); }

  /// Returns the box coordinates based on the box index in the one dimensional
  /// array
  /// @param box_idx - index of the box in boxes_
  array<uint32_t, 3> GetBoxCoordinates(size_t box_idx) const {
    array<uint32_t, 3> ret;
    ret[2] = box_idx / num_boxes_xy_;
    auto xy = box_idx % num_boxes_xy_;
    ret[1] = xy / num_boxes_axis_[0];
    ret[0] = xy % num_boxes_axis_[0];
    return ret;
  }

  /// Returns the box index in the one dimensional array based on
  /// box coordinates in space
  /// @param box_coord - box coordinates in space (x, y, z)
  size_t GetBoxIndex(array<uint32_t, 3>& box_coord) const {
    // z * num_boxes_xy_ + y * num_boxes_x + x
    return box_coord[2] * num_boxes_xy_ + box_coord[1] * num_boxes_axis_[0] +
           box_coord[0];
  }

  /// Return the box index in the one dimensional array of the box that contains
  /// the position
  size_t GetBoxIndex(const array<double, 3> position) {
    array<uint32_t, 3> box_coord;
    box_coord[0] = floor(position[0]) / box_lenght_;
    box_coord[1] = floor(position[1]) / box_lenght_;
    box_coord[2] = floor(position[2]) / box_lenght_;

    return GetBoxIndex(box_coord);
  }
};

}  // namespace bdm

#endif  // GRID_H_
