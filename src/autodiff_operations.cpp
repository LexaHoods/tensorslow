/*
* Overloaded operators and functions for the ts::Tensor class
*/

#include "../include/autodiff.hpp"


	// Overloaded arithmetic operators

template <typename T>
ts::Tensor<T> ts::operator+(const ts::Tensor<T> &x, const ts::Tensor<T> &y){
	// Element-wise sum operation

	if(
		x.wList != y.wList ||
		x.value.rows() != y.value.rows() ||
		x.value.cols() != y.value.cols()
	) {
		return ts::Tensor<T>(Eigen::Array<T, 0, 0>(), NULL);
	}


	// a = x + y
	// da / dx = 1
	// da / dy = 1

	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> grad;
	grad.setOnes(x.value.rows(), x.value.cols());

	return ts::Tensor<T>(
		x.value + y.value,
		x.wList,
		ts::Node<T>(
			{x.value.rows(), x.value.cols()}, ts::ElementWise,
			grad, x.index,
			grad, y.index
		)
	);
}



template <typename T>
ts::Tensor<T> ts::operator-(const ts::Tensor<T> &x, const ts::Tensor<T> &y){
	// Element-wise difference operation

	if(
		x.wList != y.wList ||
		x.value.rows() != y.value.rows() ||
		x.value.cols() != y.value.cols()
	) {
		return ts::Tensor<T>(Eigen::Array<T, 0, 0>(), NULL);
	}


	// a = x - y
	// da / dx = 1
	// da / dy = -1

	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> grad;
	grad.setOnes(x.value.rows(), x.value.cols());

	return ts::Tensor<T>(
		x.value - y.value,
		x.wList,
		ts::Node<T>(
			{x.value.rows(), x.value.cols()}, ts::ElementWise,
			grad, x.index,
			-1 * grad, y.index
		)
	);
}



template <typename T>
ts::Tensor<T> ts::operator*(const ts::Tensor<T> &x, const ts::Tensor<T> &y){
	// Element-wise (Hadamard) product operation

	if(
		x.wList != y.wList ||
		x.value.rows() != y.value.rows() ||
		x.value.cols() != y.value.cols()
	) {
		return ts::Tensor<T>(Eigen::Array<T, 0, 0>(), NULL);
	}


	// a = x * y
	// da / dx = y
	// da / dy = x

	return ts::Tensor<T>(
		x.value * y.value,
		x.wList,
		ts::Node<T>(
			{x.value.rows(), x.value.cols()}, ts::ElementWise,
			y.value, x.index,
			x.value, y.index
		)
	);
}



template <typename T>
ts::Tensor<T> ts::operator/(const ts::Tensor<T> &x, const ts::Tensor<T> &y){
	// Element-wise quotient operation

	if(
		x.wList != y.wList ||
		x.value.rows() != y.value.rows() ||
		x.value.cols() != y.value.cols()
	) {
		return ts::Tensor<T>(Eigen::Array<T, 0, 0>(), NULL);
	}


	// a = x / y
	// da / dx = 1 / y
	// da / dy = -x / y^2

	return ts::Tensor<T>(
		x.value + y.value,
		x.wList,
		ts::Node<T>(
			{x.value.rows(), x.value.cols()}, ts::ElementWise,
			1.0 / y.value, x.index,
			-x.value / (y.value * y.value), y.index
		)
	);
}



	// Other functions

template <typename T>
ts::Tensor<T> ts::matProd(const ts::Tensor<T> &x, const ts::Tensor<T> &y) {
	// Classic matrix-matrix product

	if(x.wList != y.wList || x.value.cols() != y.value.rows()) {
		return ts::Tensor<T>(Eigen::Array<T, 0, 0>(), NULL);
	}

	// The gradient will have to be computed for a scalar
	x.wList->elementWiseOnly = false;

	// a = x.y
	// da / dx = y^T	(transposed)
	// da / dy = x^T
	// (will be used in matrix product when computing gradient)

	return ts::Tensor<T>(
		x.value.matrix() * y.value.matrix(),
		x.wList,
		ts::Node<T>(
			{x.value.rows(), y.value.cols()}, ts::MatrixProduct,
			y.value.matrix().transpose(), x.index,
			x.value.matrix().transpose(), y.index
		)
	);
}



template <typename T>
ts::Tensor<T> ts::sigmoid(const ts::Tensor<T> &x) {
	// Element-wise sigmoid function

	// a = e^x / (e^x + 1) = 1 / (1 + e^-x)
	// da / dx = e^x / (e^x + 1)^2

	return ts::Tensor<T>(
		x.value.exp() / (x.value.exp() + 1),
		x.wList,
		ts::Node<T>(
			{x.value.rows(), x.value.cols()}, ts::ElementWise,
			x.value.exp() / (x.value.exp() + 1).pow(2), x.index
		)
	);
}



template <typename T>
ts::Tensor<T> ts::squaredNorm(const ts::Tensor<T> &x) {
	// Returns the square of the 2-norm / euclidean norm of a vector

	// The gradient will have to be computed for a scalar
	x.wList->elementWiseOnly = false;

	// a = norm()
	// da / dx = 2x

	Eigen::Array<T, 1, 1> res;
	res << (T) x.value.matrix().squaredNorm();
	return ts::Tensor<T>(
		res,
		x.wList,
		ts::Node<T>(
			{1, 1}, ts::Norm,
			2 * x.value.matrix(), x.index
		)
	);
}
