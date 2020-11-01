/*
* General automatic differentiation engine based on a Wengert list
* implementation. Reverse mode only.
*/

#pragma once

#include <vector>

#include <Eigen/Dense>



namespace ts {
	template <typename T> class Node;
	template <typename T> class WengertList;
	template <typename T> class Tensor;
	template <typename T> class Gradient;


	// This helper function allows us to create Tensor instances without
	// template syntax. This way, the type will be the same as its parent
	// WengertList.

	template <typename T>
	ts::Tensor<T> NewTensor(
		Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> newValue,
		ts::WengertList<T> * newWList
	);


	enum OperationType {
		None,
		ElementWise,
		MatrixProduct,
		Norm
	};


	template <typename T>
	ts::Tensor<T> operator+(const ts::Tensor<T> &x, const ts::Tensor<T> &y);
	template <typename T>
	ts::Tensor<T> operator-(const ts::Tensor<T> &x, const ts::Tensor<T> &y);
	template <typename T>
	ts::Tensor<T> operator*(const ts::Tensor<T> &x, const ts::Tensor<T> &y);
	template <typename T>
	ts::Tensor<T> operator/(const ts::Tensor<T> &x, const ts::Tensor<T> &y);

	template <typename T>
	ts::Tensor<T> matProd(const ts::Tensor<T> &x, const ts::Tensor<T> &y);
	template <typename T>
	ts::Tensor<T> sigmoid(const ts::Tensor<T> &x);
	template <typename T>
	ts::Tensor<T> squaredNorm(const ts::Tensor<T> &x);
}



template <typename T>
class ts::Node {
private:
	// Represents an input variable
	Node(std::vector<long> shape);

	// Represents a unary operator
	Node(std::vector<long> shape, ts::OperationType newOperationType,
		Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> xVal, int xDep
	);

	// Represents a binary operator
	Node(
		std::vector<long> shape, ts::OperationType newOperationType,
		Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> xVal, int xDep,
		Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> yVal, int yDep
	);


	std::vector< Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> > values{};
	std::vector<int> dependencies{};

	// Shape of the corresponding tensor
	unsigned rows, cols;
	ts::OperationType operationType = ts::None;

public:

	friend ts::Tensor<T>;

	friend ts::Tensor<T> operator+<>(const ts::Tensor<T> &x, const ts::Tensor<T> &y);
	friend ts::Tensor<T> operator-<>(const ts::Tensor<T> &x, const ts::Tensor<T> &y);
	friend ts::Tensor<T> operator*<>(const ts::Tensor<T> &x, const ts::Tensor<T> &y);
	friend ts::Tensor<T> operator/<>(const ts::Tensor<T> &x, const ts::Tensor<T> &y);

	friend ts::Tensor<T> matProd<>(const ts::Tensor<T> &x, const ts::Tensor<T> &y);
	friend ts::Tensor<T> sigmoid<>(const ts::Tensor<T> &x);
	friend ts::Tensor<T> squaredNorm<>(const ts::Tensor<T> &x);
};



template <typename T>
class ts::WengertList {
private:
	std::vector<ts::Node<T>> nodes{};
	bool elementWiseOnly = true;

public:
	int size();

	friend class ts::Tensor<T>;

	// Other non-element wise operations (to change elementWiseOnly)
	friend ts::Tensor<T> matProd<>(const ts::Tensor<T> &x, const ts::Tensor<T> &y);
	friend ts::Tensor<T> sigmoid<>(const ts::Tensor<T> &x);
	friend ts::Tensor<T> squaredNorm<>(const ts::Tensor<T> &x);
};



template <typename T>
class ts::Tensor {
private:
	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> value;
	ts::WengertList<T> * wList;
	int index;

	// We want this constructor to be private as it is supposed to be called by
	// our friends overloaded operators and functions only. This constructor
	// thus allows us to create a Tensor with dependencies in the Wengert list.
	Tensor(
		Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> newValue,
		ts::WengertList<T> * newWList, ts::Node<T> node
	);

	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> incrementGradient(
			Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> * childDerivative,
			ts::Node<T> * node, unsigned j
	);

public:
	Tensor(
		Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> newValue,
		ts::WengertList<T> * wList
	);

	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> getValue();
	ts::Gradient<T> grad();

	friend ts::Gradient<T>;

	friend ts::Tensor<T> operator+<>(const ts::Tensor<T> &x, const ts::Tensor<T> &y);
	friend ts::Tensor<T> operator-<>(const ts::Tensor<T> &x, const ts::Tensor<T> &y);
	friend ts::Tensor<T> operator*<>(const ts::Tensor<T> &x, const ts::Tensor<T> &y);
	friend ts::Tensor<T> operator/<>(const ts::Tensor<T> &x, const ts::Tensor<T> &y);

	friend ts::Tensor<T> matProd<>(const ts::Tensor<T> &x, const ts::Tensor<T> &y);
	friend ts::Tensor<T> sigmoid<>(const ts::Tensor<T> &x);
	friend ts::Tensor<T> squaredNorm<>(const ts::Tensor<T> &x);
};

template <typename T>
ts::Tensor<T> NewTensor(
	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> newValue,
	ts::WengertList<T> * newWList
);



template <typename T>
class ts::Gradient {
private:
	// Constructor is private since we want instances of this class to be
	// generated by the Tensor::grad() method only
	Gradient(
		std::vector< Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> > newDerivatives
	);

	std::vector< Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> > derivatives;

public:
	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> getValue(ts::Tensor<T> a);
	bool isEmpty();

	friend class ts::Tensor<T>;
};
