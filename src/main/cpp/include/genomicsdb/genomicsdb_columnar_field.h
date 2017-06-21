/**
 * The MIT License (MIT)
 * Copyright (c) 2016 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of 
 * this software and associated documentation files (the "Software"), to deal in 
 * the Software without restriction, including without limitation the rights to 
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of 
 * the Software, and to permit persons to whom the Software is furnished to do so, 
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all 
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS 
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR 
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER 
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN 
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef GENOMICSDB_COLUMNAR_FIELD_H
#define GENOMICSDB_COLUMNAR_FIELD_H

#include "headers.h"
#include "vcf.h"
#include "gt_common.h"

class GenomicsDBColumnarFieldException : public std::exception {
  public:
    GenomicsDBColumnarFieldException(const std::string m) : msg_(m) { ; }
    ~GenomicsDBColumnarFieldException() { ; }
    // ACCESSORS
    /** Returns the exception message. */
    const char* what() const noexcept { return msg_.c_str(); }
  private:
    std::string msg_;
};

/*
 * Think of this class as a vector which cannot be resized
 * The buffer space of this object is owned by GenomicsDBColumnarField - this object
 * only contains offsets within the buffer
 */
template<class DataType>
class LinkedVector
{

};
//ceil(buffer_size/field_size)*field_size
#define GET_ALIGNED_BUFFER_SIZE(buffer_size, field_size) ((((buffer_size)+(field_size)-1u)/(field_size))*(field_size))

class GenomicsDBBuffer
{
  public:
    GenomicsDBBuffer(const size_t num_bytes)
    {
      m_is_in_live_list = false;
      m_buffer.resize(num_bytes);
      m_valid.resize(num_bytes);
      m_num_live_entries = 0ull;
      m_num_filled_entries = 0ull;
      m_num_unprocessed_entries = 0ull;
      m_next_buffer = 0;
      m_previous_buffer = 0;
    }
    //Delete both move and copy constructors
    GenomicsDBBuffer(const GenomicsDBBuffer& other) = delete;
    GenomicsDBBuffer(GenomicsDBBuffer&& other) = delete;
    //Functions
    inline void set_is_in_live_list(const bool val) { m_is_in_live_list = val; }
    inline bool is_in_live_list() const { return m_is_in_live_list; }
    //Live and filled entry control
    inline size_t get_num_live_entries() const { return m_num_live_entries; }
    inline void set_num_live_entries(const size_t n)
    {
      m_num_live_entries = n;
      m_num_filled_entries = n;
      m_num_unprocessed_entries = n;
    }
    inline void decrement_num_live_entries(const size_t value)
    {
      assert(m_num_live_entries >= value);
      m_num_live_entries -= value;
    }
    inline void increment_num_live_entries(const size_t val=1u)
    {
      m_num_live_entries += val;
    }
    inline size_t get_num_unprocessed_entries() const
    {
      return m_num_unprocessed_entries;
    }
    inline void decrement_num_unprocessed_entries(const size_t value)
    {
      assert(m_num_unprocessed_entries >= value);
      m_num_unprocessed_entries -= value;
    }
    inline std::vector<uint8_t>& get_buffer() { return m_buffer; }
    inline const std::vector<uint8_t>& get_buffer() const { return m_buffer; }
    inline uint8_t* get_buffer_pointer() { return &(m_buffer[0]); }
    inline const uint8_t* get_buffer_pointer() const { return &(m_buffer[0]); }
    inline size_t get_buffer_size_in_bytes() const { return m_buffer.size(); }
    /*inline std::vector<size_t>& get_offsets() { return m_offsets; }*/
    inline size_t* get_offsets_pointer() { return &(m_offsets[0]); }
    inline size_t get_offsets_size_in_bytes() const { return ((m_offsets.size()-1u)*sizeof(size_t)); } //last offsets element holds the "size"
    inline size_t get_offsets_length() const { return (m_offsets.size()-1u); } //last offsets element holds the "size"
    /*
     * Set offset in buffer of the idx-th element
     * Useful only for variable length fields and used by the iterators
     * This allows us to use m_offsets for computing the size of every cell data without
     * having to write "if(last_cell) else" expressions
     */
    inline void set_offset(const size_t idx, const size_t value)
    {
      assert(idx < m_offsets.size());
      m_offsets[idx] = value;
    }
    inline size_t get_offset(const size_t idx) const
    {
      assert(idx < m_offsets.size() && idx < m_num_filled_entries);
      return m_offsets[idx ];
    }
    inline size_t get_size_of_variable_length_field(const size_t idx) const
    {
      assert(idx+1u < m_offsets.size() && idx < m_num_filled_entries);
      return m_offsets[idx+1u] - m_offsets[idx];
    }
    inline bool is_valid(const size_t idx) const
    {
      assert(idx < m_valid.size() && idx < m_num_filled_entries);
      return m_valid[idx];
    }
    inline std::vector<bool>& get_valid_vector() { return m_valid; }
    //Buffer pointers
    inline void set_next_buffer(GenomicsDBBuffer* next) { m_next_buffer = next; }
    inline void set_previous_buffer(GenomicsDBBuffer* previous) { m_previous_buffer = previous; }
    inline GenomicsDBBuffer* get_next_buffer() { return m_next_buffer; }
    inline GenomicsDBBuffer* get_previous_buffer() { return m_previous_buffer; } 
  protected:
    //bool used only in assertions - for debugging
    bool m_is_in_live_list;
    std::vector<uint8_t> m_buffer;
    std::vector<bool> m_valid;
    //For variable length fields, stores the offsets
    //If the buffer has data from N-1 cells, this vector has N elements
    //The total size of the variable length field is always stored in the Nth element
    //This allows us to use m_offsets for computing the size of every cell data without
    //having to write "if(last_cell) else" expressions
    std::vector<size_t> m_offsets;
    //m_num_filled_entries - the #items filled in the buffer from TileDB
    //m_num_unprocessed_entries - a counter that's decremented by iterators
    //m_num_live_entries <= m_num_filled_entries
    //m_num_unprocessed_entries <= m_num_filled_entries
    size_t m_num_live_entries;
    size_t m_num_filled_entries;
    size_t m_num_unprocessed_entries;
    //Pointers in the linked list of buffers
    GenomicsDBBuffer* m_next_buffer;
    GenomicsDBBuffer* m_previous_buffer;
};

typedef GenomicsDBBuffer GenomicsDBFixedSizeFieldBuffer;

class GenomicsDBVariableSizeFieldBuffer: public GenomicsDBBuffer
{
  public:
    GenomicsDBVariableSizeFieldBuffer(const size_t num_bytes)
      : GenomicsDBBuffer(num_bytes)
    {
      m_offsets.resize(GET_ALIGNED_BUFFER_SIZE(num_bytes, sizeof(size_t))/sizeof(size_t), 0ull);
    }
};

//Printer
//Partial class specialization is allowed, but partial function template specialization isn't
template<typename T, bool print_as_list>
class GenomicsDBColumnarFieldPrintOperator
{
  public:
    static void print(std::ostream& fptr, const uint8_t* ptr, const size_t num_elements);
};

/*
 * Class that stores data for a given field from multiple VariantCalls/TileDB cells together in a columnar fashion
 * Maintains a big buffer for data, valid bitvector and offsets array
 */
class GenomicsDBColumnarField
{
  public:
    GenomicsDBColumnarField(const std::type_index element_type, const int length_descriptor,
        const unsigned fixed_length_field_length, const size_t num_bytes);
    ~GenomicsDBColumnarField();
    //Delete copy constructor
    GenomicsDBColumnarField(const GenomicsDBColumnarField& other) = delete;
    //Define move constructor
    GenomicsDBColumnarField(GenomicsDBColumnarField&& other);
    void clear()
    {
    }
    //Static functions - function pointers hold addresses to one of these functions
    //Check validity
    template<typename T>
    static bool check_tiledb_valid_element(const uint8_t* ptr, const size_t num_elements)
    {
      auto data = reinterpret_cast<const T*>(ptr);
      for(auto i=0ull;i<num_elements;++i)
        if(!is_tiledb_missing_value<T>(data[i]))
          return true;
      return false;
    }
    //Buffer pointer handling
    GenomicsDBBuffer* get_free_buffer()
    {
      if(m_free_buffer_list_head_ptr == 0)
        add_new_buffer();
      return m_free_buffer_list_head_ptr;
    }
    GenomicsDBBuffer* get_live_buffer_list_head_ptr() { return m_live_buffer_list_head_ptr; }
    GenomicsDBBuffer* get_live_buffer_list_tail_ptr() { return m_live_buffer_list_tail_ptr; }
    const GenomicsDBBuffer* get_live_buffer_list_head_ptr() const { return m_live_buffer_list_head_ptr; }
    const GenomicsDBBuffer* get_live_buffer_list_tail_ptr() const { return m_live_buffer_list_tail_ptr; }
    void move_buffer_to_live_list(GenomicsDBBuffer* buffer);
    void move_buffer_to_free_list(GenomicsDBBuffer* buffer);
    void set_valid_vector_in_live_buffer_list_tail_ptr();
    //Field lengths
    bool is_variable_length_field() const { return (m_length_descriptor != BCF_VL_FIXED); }
    inline size_t get_fixed_length_field_size_in_bytes() const { return m_fixed_length_field_size; }
    //Functions to update index in live list tail buffer
    inline void set_curr_index_in_live_list_tail(const size_t val) { m_curr_index_in_live_buffer_list_tail = val; }
    inline void advance_curr_index_in_live_list_tail(const size_t val=1u) { m_curr_index_in_live_buffer_list_tail += val; }
    inline size_t get_curr_index_in_live_list_tail() const { return m_curr_index_in_live_buffer_list_tail; }
    //Get pointer to data in GenomicsDBBuffer*
    inline const uint8_t* get_pointer_to_data_in_buffer_at_index(const GenomicsDBBuffer* buffer_ptr,
        const size_t index) const
    {
      assert(buffer_ptr);
      if(m_length_descriptor == BCF_VL_FIXED)
        return buffer_ptr->get_buffer_pointer() + (m_fixed_length_field_size*index);
      else
        return buffer_ptr->get_buffer_pointer() + buffer_ptr->get_offset(index);
    }
    inline uint8_t* get_pointer_to_data_in_buffer_at_index(GenomicsDBBuffer* buffer_ptr,
        const size_t index)
    {
      assert(buffer_ptr);
      if(m_length_descriptor == BCF_VL_FIXED)
        return buffer_ptr->get_buffer_pointer() + (m_fixed_length_field_size*index);
      else
        return buffer_ptr->get_buffer_pointer() + buffer_ptr->get_offset(index);
    }
    //Get num bytes for current idx in buffer
    inline size_t get_size_of_data_in_buffer_at_index(const GenomicsDBBuffer* buffer_ptr,
        const size_t index) const
    {
      assert(buffer_ptr);
      if(m_length_descriptor == BCF_VL_FIXED)
        return m_fixed_length_field_size;
      else
        return buffer_ptr->get_size_of_variable_length_field(index);
    }
    inline size_t get_length_of_data_in_buffer_at_index(const GenomicsDBBuffer* buffer_ptr,
        const size_t index) const
    {
      return get_size_of_data_in_buffer_at_index(buffer_ptr, index)/m_element_size;
    }
    inline bool is_valid_data_in_buffer_at_index(const GenomicsDBBuffer* buffer_ptr,
        const size_t index) const
    {
      return buffer_ptr->is_valid(index);
    }
    void print_data_in_buffer_at_index(std::ostream& fptr,
        const GenomicsDBBuffer* buffer_ptr, const size_t index) const;
    void print_ALT_data_in_buffer_at_index(std::ostream& fptr,
        const GenomicsDBBuffer* buffer_ptr, const size_t index) const;
  private:
    void copy_simple_members(const GenomicsDBColumnarField& other);
    void assign_function_pointers();
    template<bool print_as_list>
    void assign_print_function_pointers(VariantFieldTypeEnum variant_enum_type);
    void add_new_buffer()
    {
      auto buffer_ptr = create_new_buffer();
      if(m_free_buffer_list_head_ptr)
      {
        assert(m_free_buffer_list_head_ptr->get_previous_buffer() == 0);
        m_free_buffer_list_head_ptr->set_previous_buffer(buffer_ptr);
        buffer_ptr->set_next_buffer(m_free_buffer_list_head_ptr);
      }
      m_free_buffer_list_head_ptr = buffer_ptr;
    }
    GenomicsDBBuffer* create_new_buffer() const
    {
      return (m_length_descriptor == BCF_VL_FIXED) ? (new GenomicsDBFixedSizeFieldBuffer(m_buffer_size))
        : (new GenomicsDBVariableSizeFieldBuffer(m_buffer_size));
    }
  private:
    int m_length_descriptor;
    unsigned m_fixed_length_field_num_elements;
    unsigned m_fixed_length_field_size;
    unsigned m_element_size;
    std::type_index m_element_type;
    //Function pointers - assigned based on type of data
    //Function pointer that determines validity check
    bool (*m_check_tiledb_valid_element)(const uint8_t* ptr, const size_t num_elements);
    void (*m_print)(std::ostream& fptr, const uint8_t* ptr, const size_t num_elements);
    size_t m_buffer_size;
    //Head of list containing free buffers - can be used in invocation of tiledb_array_read()
    GenomicsDBBuffer* m_free_buffer_list_head_ptr;
    //Head of list containing buffers with live data - list is in column-major order
    GenomicsDBBuffer* m_live_buffer_list_head_ptr;
    //Tail of list containing buffers with live data - list is in column-major order
    //tail needed because new data gets appended to end
    GenomicsDBBuffer* m_live_buffer_list_tail_ptr;
    //Index of the element in the live buffer list head that must be read next
    size_t m_curr_index_in_live_buffer_list_tail;
};

#endif
