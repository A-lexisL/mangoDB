#ifndef SRC_DB_TABLE_H_
#define SRC_DB_TABLE_H_

#include <cstddef>
#include <iosfwd>
#include <iterator>
#include <limits>
#include <memory>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../utils/formatter.h"
#include "../utils/uexception.h"

class Table {
public:
  using KeyType = std::string;
  using FieldNameType = std::string;
  using FieldIndex = std::size_t;
  using ValueType = int;
  static constexpr const ValueType ValueTypeMax =
      std::numeric_limits<ValueType>::max();
  static constexpr const ValueType ValueTypeMin =
      std::numeric_limits<ValueType>::min();
  using SizeType = std::size_t;

private:
  mutable std::shared_mutex dataMutex;
  /** A row in the table */
  class Datum {
    friend class Table;
    friend auto operator<<(std::ostream &output, const Table &table)
        -> std::ostream &;
    /** Unique key of this datum */
    KeyType key;
    /** The values in the order of fields */
    std::vector<ValueType> datum;

  public:
    Datum() = default;
    explicit Datum(const SizeType &size) : datum(size, ValueType()) {}
    template <class ValueTypeContainer>
    explicit Datum(KeyType key, const ValueTypeContainer &datum)
        : key(std::move(key)), datum(datum) {}
    explicit Datum(KeyType key, std::vector<ValueType> &&datum)
        : key(std::move(key)), datum(std::move(datum)) {}
  };
  using DataIterator = std::vector<Datum>::iterator;
  using ConstDataIterator = std::vector<Datum>::const_iterator;
  /** The fields, ordered as defined in fieldMap */
  std::vector<FieldNameType> fields;
  /** Map field name into index */
  std::unordered_map<FieldNameType, FieldIndex> fieldMap;
  /** The rows are saved in a vector, which is unsorted */
  std::vector<Datum> data;
  /** Used to keep the keys unique and provide O(1) access with key */
  std::unordered_map<KeyType, SizeType> keyMap;
  /** The name of table */
  std::string tableName;
  // The temporary guard stays alive while the member initializers copy rows.
  Table(std::string name, const Table &origin,
        const std::shared_lock<std::shared_mutex> & /*sourceLock*/)
      : fields(origin.fields), fieldMap(origin.fieldMap), data(origin.data),
        keyMap(origin.keyMap), tableName(std::move(name)) {}

public:
  using Ptr = std::unique_ptr<Table>;
  // Hold a shared lock while reading and a unique lock while changing rows.
  auto mutex() const -> std::shared_mutex & { return dataMutex; }

  /**
   * A proxy class that provides abstraction on internal Implementation.
   * Allows independent variation on the representation for a table object
   */
  template <class Iterator, class VType> class ObjectImpl {
    friend class Table;
    using TableType =
        std::conditional_t<std::is_const_v<VType>, const Table, Table>;
    /** Not const because key can be updated */
    Iterator iterator;
    TableType *table;

  public:
    using Ptr = std::unique_ptr<ObjectImpl>;
    ObjectImpl(Iterator datumIt, TableType *owner)
        : iterator(datumIt), table(owner) {}
    ObjectImpl(const ObjectImpl &) = default;
    ObjectImpl(ObjectImpl &&) noexcept = default;
    auto operator=(const ObjectImpl &) -> ObjectImpl & = default;
    auto operator=(ObjectImpl &&) noexcept -> ObjectImpl & = default;
    ~ObjectImpl() = default;
    // The reference remains valid while the underlying row remains valid.
    [[nodiscard]] auto key() const -> const KeyType & { return iterator->key; }
    void setKey(KeyType key) {
      auto const keyMapIt = table->keyMap.find(iterator->key);
      auto const dataIt = std::move(keyMapIt->second);
      table->keyMap.erase(keyMapIt);
      table->keyMap.emplace(key, std::move(dataIt));
      iterator->key = std::move(key);
    }

    /**
     * Accessing by index should be, at least as fast as accessing by field
     * name. Clients should prefer accessing by index if the same field is
     * accessed frequently (the implement is improved so that index is actually
     * faster now)
     */
    [[nodiscard]] auto get(const FieldNameType &field) const -> VType & {
      try {
        const auto &index = table->fieldMap.at(field);
        return iterator->datum.at(index);
      } catch (const std::out_of_range &) {
        throw TableFieldNotFound(R"(Field name "?" doesn't exists.)"_f % field);
      }
    }
    [[nodiscard]] auto get(const FieldIndex &index) const -> VType & {
      try {
        return iterator->datum.at(index);
      } catch (const std::out_of_range &) {
        throw TableFieldNotFound("Field index ? out of range."_f % index);
      }
    }
    auto operator[](const FieldNameType &field) const -> VType & {
      return get(field);
    }
    auto operator[](const FieldIndex &index) const -> VType & {
      return get(index);
    }
  };
  using Object = ObjectImpl<DataIterator, ValueType>;
  using ConstObject = ObjectImpl<ConstDataIterator, const ValueType>;

  /** A proxy class that provides iteration on the table */
  template <typename ObjType, typename DatumIterator> class IteratorImpl {
    using TablePointer =
        std::conditional_t<std::is_same_v<DatumIterator, ConstDataIterator>,
                           const Table *, Table *>;
    using difference_type = std::ptrdiff_t;
    using value_type = ObjType;
    using pointer = ObjType::Ptr;
    using reference = ObjType;
    using iterator_category = std::random_access_iterator_tag;
    // See https://stackoverflow.com/questions/37031805/
    friend class Table;
    DatumIterator iterator;
    TablePointer table = nullptr;

  public:
    IteratorImpl(DatumIterator datumIt, TablePointer owner)
        : iterator(datumIt), table(owner) {}
    IteratorImpl() = default;
    IteratorImpl(const IteratorImpl &) = default;
    IteratorImpl(IteratorImpl &&) noexcept = default;
    auto operator=(const IteratorImpl &) -> IteratorImpl & = default;
    auto operator=(IteratorImpl &&) noexcept -> IteratorImpl & = default;
    ~IteratorImpl() = default;
    auto operator->() -> pointer { return createProxy(iterator, table); }
    auto operator*() -> reference { return *createProxy(iterator, table); }
    auto operator+(int n) -> IteratorImpl {
      return IteratorImpl(iterator + n, table);
    }
    auto operator-(int n) -> IteratorImpl {
      return IteratorImpl(iterator - n, table);
    }
    auto operator+=(int n) -> IteratorImpl & {
      iterator += n;
      return *this;
    }
    auto operator-=(int n) -> IteratorImpl & {
      iterator -= n;
      return *this;
    }
    auto operator++() -> IteratorImpl & {
      ++iterator;
      return *this;
    }
    auto operator--() -> IteratorImpl & {
      --iterator;
      return *this;
    }
    auto operator++(int) -> IteratorImpl {
      auto retVal = IteratorImpl(*this);
      ++iterator;
      return retVal;
    }
    auto operator--(int) -> IteratorImpl {
      auto retVal = IteratorImpl(*this);
      --iterator;
      return retVal;
    }

    auto operator==(const IteratorImpl &other) -> bool {
      return this->iterator == other.iterator;
    }
    auto operator!=(const IteratorImpl &other) -> bool {
      return this->iterator != other.iterator;
    }
    auto operator<=(const IteratorImpl &other) -> bool {
      return this->iterator <= other.iterator;
    }
    auto operator>=(const IteratorImpl &other) -> bool {
      return this->iterator >= other.iterator;
    }
    auto operator<(const IteratorImpl &other) -> bool {
      return this->iterator < other.iterator;
    }
    auto operator>(const IteratorImpl &other) -> bool {
      return this->iterator > other.iterator;
    }
  };

  using Iterator = IteratorImpl<Object, decltype(data.begin())>;
  using ConstIterator = IteratorImpl<ConstObject, decltype(data.cbegin())>;

private:
  static auto createProxy(ConstDataIterator iterator, const Table *table)
      -> ConstObject::Ptr {
    return std::make_unique<ConstObject>(iterator, table);
  }

  static auto createProxy(DataIterator iterator, Table *table) -> Object::Ptr {
    return std::make_unique<Object>(iterator, table);
  }

  void pop_back() {
    const KeyType &key = data.at(data.size() - 1).key;
    keyMap.erase(key);
    data.pop_back();
  }
  auto swap_row_in_data(SizeType id1, SizeType id2) -> bool {
    if (id1 == id2) {
      return true;
    }
    if (id1 >= data.size() || id2 >= data.size()) {
      return false;
    }
    std::swap(data.at(id1), data.at(id2));
    keyMap[data.at(id1).key] = id1;
    keyMap[data.at(id2).key] = id2;
    return true;
  }

public:
  Table() = delete;
  explicit Table(std::string name) : tableName(std::move(name)) {}
  /** Accept any container that contains string. */
  template <class FieldIDContainer>
  Table(const std::string &name, const FieldIDContainer &fields);
  /** Copy constructor from another table */
  Table(std::string name, const Table &origin)
      : Table(std::move(name), origin, std::shared_lock(origin.dataMutex)) {}
  auto getFieldIndex(const FieldNameType &field) const -> FieldIndex;
  auto eraseByKey(const KeyType &key) -> bool;
  auto ifKeyExist(const KeyType &key) const -> bool;
  void insertByIndex(const KeyType &key, std::vector<ValueType> &&data);
  auto operator[](const KeyType &key) -> Object::Ptr { return findRow(key); }
  auto findRow(const KeyType &key) -> Object::Ptr;
  void setName(std::string name) { this->tableName = std::move(name); }
  auto name() const -> const std::string & { return this->tableName; }
  auto empty() const -> bool { return this->data.empty(); }
  auto size() const -> std::size_t { return this->data.size(); }
  auto field() const -> const std::vector<FieldNameType> & {
    return this->fields;
  }
  auto clear() -> std::size_t {
    auto const result = keyMap.size();
    data.clear();
    keyMap.clear();
    return result;
  }
  auto begin() -> Iterator { return {data.begin(), this}; }
  auto end() -> Iterator { return {data.end(), this}; }
  auto begin() const -> ConstIterator { return {data.cbegin(), this}; }
  auto end() const -> ConstIterator { return {data.cend(), this}; }
  friend auto operator<<(std::ostream &output, const Table &table)
      -> std::ostream &;
};

auto operator<<(std::ostream &output, const Table &table) -> std::ostream &;

template <class FieldIDContainer>
Table::Table(const std::string &name, const FieldIDContainer &fields)
    : fields(fields.cbegin(), fields.cend()), tableName(name) {
  SizeType index = 0;
  for (const auto &fieldName : fields) {
    if (fieldName == "KEY") {
      throw MultipleKey("Error creating table \"" + name +
                        "\": Multiple KEY field.");
    }
    fieldMap.emplace(fieldName, index++);
  }
}

#endif  // SRC_DB_TABLE_H_
