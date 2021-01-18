/*
* Implementation for AD of convolution-related operations, that will typically
* be used in a CNN.
*/

#include "../include/convolution.hpp"



	// Convolution operation on Eigen arrays
	// (will be reused in ts::Tensor convolutions)

template <typename T>
Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> ts::convArray(
	const Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> &mat,
	const Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> &ker
) {

	// This is a basic and slow implementation of convolution. It is not used
	// anymore in our CNN model. For a more efficient version, see the im2col
	// version.

	// Make sure kernel is smaller
	if(
		mat.rows() < ker.rows() ||
		mat.cols() < ker.cols()
	) {
		return Eigen::Array<T, 0, 0>();
	}

	unsigned newRows = mat.rows() - ker.rows() + 1;
	unsigned newCols = mat.cols() - ker.cols() + 1;


	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> res;
	res.resize(newRows, newCols);

	#pragma omp parallel for collapse(2)
	for(unsigned i=0; i<newCols; i++) {
		for(unsigned j=0; j<newRows; j++) {
			// Compute one element of feature map
			res(j, i) =
			(mat.block(j, i, ker.rows(), ker.cols()) * ker).sum();
		}
	}

	return res;
}



template <typename T>
Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> ts::im2conv(
	const Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> &mat,
	const Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> &ker
) {

	// This is a version of convolution using the im2col approach.
	// Even though this method is considered more
	// computationally efficient, it requires more memory.

	// Make sure ker is smaller than mat
	if(mat.rows() < ker.rows() || mat.cols() < ker.cols()) {
		return Eigen::Array<T, 0, 0>();
	}


		// 1) Use im2col method to convert mat

	unsigned newRows = mat.rows() - ker.rows() + 1;
	unsigned newCols = mat.cols() - ker.cols() + 1;

	// Flatten and get new kernel height
	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> vertKer = ker;
	vertKer = Eigen::Map<Eigen::Array<T, -1, 1>>(
		vertKer.data(), vertKer.cols() * vertKer.rows()
	);
	unsigned height = vertKer.rows();


	// Generate res matrix for product
	// RowMajor is needed for resize at the end
	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> res;
	res.setZero(
		newRows * newCols,
		height
	);


	#pragma omp parallel for collapse(2)
	for(unsigned i=0; i<newCols; i++) {
		for(unsigned j=0; j<newRows; j++) {
			Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> tmp;

			tmp = mat.block(j, i, ker.rows(), ker.cols());
			tmp = Eigen::Map<Eigen::Array<T, 1, -1>>(
				tmp.data(), tmp.cols()*tmp.rows()
			);
			res.block(j * newCols + i, 0, 1, height) = tmp;
		}
	}



		// 2) Compute matrix product

	res = res.matrix() * vertKer.matrix();
	res.resize(newRows, newCols);

	return res;
}



	// Convolution

template <typename T>
Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> ts::ConvolutionNode<T>::incrementGradient(
		Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> &childDerivative,
		unsigned &j
) {

	// Used in the  ts::Tensor::grad() method. Computes the increment of a derivative
	// for a convolution operation.

	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> increment;

	// Matrices are already prepared at this stage, so we only need to put the
	// operands in the correct order for convolution.

	if(
		childDerivative.rows() > this->values[j].rows() &&
		childDerivative.rows() > this->values[j].cols()
	) {
		increment = ts::convArray(childDerivative, this->values[j]);
	} else {
		increment = ts::convArray(this->values[j], childDerivative);
	}

	return increment;
}



template <typename T>
ts::Tensor<T> ts::convolution(const ts::Tensor<T> &mat, const ts::Tensor<T> &ker) {
	// Convolution operation
	// Resulting matrix is of size : (mat.x - ker.x + 1, mat.y - ker.y + 1)
	// (where mat.x >= ker.x and mat.y >= mat.y)

	if(
		mat.wList != ker.wList ||
		mat.value.rows() < ker.value.rows() ||
		mat.value.cols() < ker.value.cols()
	) {
		return ts::Tensor<T>(Eigen::Array<T, 0, 0>(), NULL);
	}

	// The gradient will have to be computed for a scalar
	mat.wList->elementWiseOnly = false;


	// Compute res
	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> res = ts::convArray(
		mat.value, ker.value
	);

	// Init dMat matrix (for matrix partial derivative)
	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> dMat;
	dMat.setZero(
		2 * res.rows() + ker.value.rows() - 2,
		2 * res.cols() + ker.value.cols() - 2
	);

	dMat.block(
		res.rows() - 1,
		res.cols() - 1,
		ker.value.rows(), ker.value.cols()
	) = ker.value.rowwise().reverse().colwise().reverse();

	std::shared_ptr<ts::Node<T>> nodePtr (
		new ts::ConvolutionNode<T>(
			{res.rows(), res.cols()},
			dMat, mat.index,
			mat.value, ker.index
		)
	);

	return ts::Tensor<T>(res, mat.wList, nodePtr);
}



	// Max pooling

template <typename T>
ts::PoolingNode<T>::PoolingNode(
	std::vector<long> shape,
	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> xVal, int xDep,
	std::vector<unsigned> newPool
) {

	// PoolingNode specific constructor to store the size of pools
	// (this allows us to easily upscale the matrix in grad computation)

	this->rows = shape[0];
	this->cols = shape[1];

	this->values =  {xVal};	// [da/dx]
	this->dependencies =  {xDep};

	pool = newPool;
}



template <typename T>
Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> ts::PoolingNode<T>::incrementGradient(
	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> &childDerivative,
	unsigned &j
) {

	// Used in the  ts::Tensor::grad() method. Computes the increment of a derivative
	// for a max pooling / downsample operation.


	// Upsample matrix of child derivative to match original size

	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> upsample;
	upsample.setZero(this->values[j].rows(), this->values[j].cols());


	// Affect coefficients of childDerivative to upsample pools by filling each
	// pool with the corresponding value

	for(unsigned i=0; i<childDerivative.cols(); i++) {
		for(unsigned j=0; j<childDerivative.rows(); j++) {

			// Fill one pool with one value
			for(unsigned k=0; k<pool[1]; k++) {
				for(unsigned l=0; l<pool[0]; l++) {
					upsample(j * pool[0] + l, i * pool[1] + k) =
					childDerivative(j, i);
				}
			}

		}
	}


	// Compute & return element-wise product with the this->values
	// (since this->values is 0/1-flled, we will only get the coefficients in
	// the desired positions, and 0 anywhere else)
	return upsample * this->values[j];
}



template <typename T>
ts::Tensor<T> ts::maxPooling(const ts::Tensor<T> &x, std::vector<unsigned> pool) {
	// Max pooling operation : we keep only the biggest element in each pool
	// in order to reduce the size of a matrix
	// Resulting matrix is of size : (mat.x / pool.x, mat.y / pool.y)
	// (where mat.x >= pool.x and mat.y >= pool.y)

	if(pool.size() != 2) {
		return ts::Tensor<T>(Eigen::Array<T, 0, 0>(), NULL);
	}

	if(
		x.value.rows() % pool[0] != 0 ||
		x.value.cols() % pool[1] != 0
	) {
		return ts::Tensor<T>(Eigen::Array<T, 0, 0>(), NULL);
	}

	// The gradient will have to be computed for a scalar
	x.wList->elementWiseOnly = false;


	// Init result
	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> res;
	res.setZero(x.value.rows() / pool[0], x.value.cols() / pool[1]);


	// Init dx
	// (dx is 1 for each max element, 0 elsewhere)
	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> dx;
	dx.setZero(x.value.rows(), x.value.cols());


	unsigned xMax, yMax;
	T maxVal;


	// Compute both pooled matrix (res) and dx
	for(unsigned i=0; i<res.cols(); i++) {
		for(unsigned j=0; j<res.rows(); j++) {

			// Get index of pool's max element
			// (for now it seems the best way is to manually iterate over
			// elements)

			xMax = j * pool[0];
			yMax = i * pool[1];
			maxVal = x.value(j * pool[0], i * pool[1]);

			for(unsigned k=0; k<pool[1]; k++) {
				for(unsigned l=0; l<pool[0]; l++) {

					if(x.value(j * pool[1] + l, i * pool[0] + k) > maxVal) {
						maxVal = x.value(j * pool[1] + l, i * pool[0] + k);
						xMax = j * pool[1] + l;
						yMax = i * pool[0] + k;
					}

				}
			}

			// Assigning values for result and derivative
			res(j, i) = maxVal;
			dx(xMax, yMax) = 1.0;

		}
	}


	std::shared_ptr<ts::Node<T>> nodePtr (
		new ts::PoolingNode<T>(
			{res.rows(), res.cols()},
			dx, x.index,
			pool
		)
	);

	return ts::Tensor<T>(res, x.wList, nodePtr);
}



// Vertical concatenation

template <typename T>
ts::VertCatNode<T>::VertCatNode(
	std::vector<long> shape,
	std::vector<int> newDependencies,
	std::vector<long> newHeights
) {

	// VertCatNode specific constructor to store the height of first matrix
	// This way we can copy correct elements in inrementGradient

	// New tensor shape (vector)
	this->rows = shape[0];
	this->cols = shape[1];

	this->dependencies =  newDependencies;

	// Height of the first matrix
	heights = newHeights;
}



template <typename T>
Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> ts::VertCatNode<T>::incrementGradient(
	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> &childDerivative,
	unsigned &j
) {

	// Used in the  ts::Tensor::grad() method. Computes the increment of a derivative
	// for a matrix flattening.

	// childDerivative is a flattened vector. We need to convert it back to a
	// matrix with the dimensions of the original matrix.

	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> mat;
	mat.resize(heights[j+1] - heights[j], childDerivative.cols());

	mat = childDerivative.block(
		heights[j], 0,
		heights[j+1] - heights[j], childDerivative.cols()
	);


	return mat;
}



template <typename T>
ts::Tensor<T> ts::vertCat(const std::vector<ts::Tensor<T>> &x) {
	// Vertical concatenation operation
	// x[i] will be under x[i-1]

	if(x.size() == 0) {
		return ts::Tensor<T>(Eigen::Array<T, 0, 0>(), NULL);
	}


	// The gradient will have to be computed for a scalar
	x[0].wList->elementWiseOnly = false;


	// Compute size of resulting matrix, and storing each input matrix position
	// We will also make sure that all matrices have the same width / wList

	std::vector<long> heights = {0}; // WARNING Values are cumulative starting heights
	long height = 0;
	long width = x[0].value.cols();
	std::vector<int> dependencies = {};

	for(unsigned i=0; i<x.size(); i++) {

		if(x[i].value.cols() != width || x[i].wList != x[0].wList) {
			return ts::Tensor<T>(Eigen::Array<T, 0, 0>(), NULL);
		}

		height += x[i].value.rows();
		heights.push_back(height);
		dependencies.push_back(x[i].index);

	}


	// Set res vector
	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> res;
	res.resize(height, width);

	for(unsigned i=0; i<x.size(); i++) {
		res.block(heights[i], 0, heights[i+1] - heights[i], width) = x[i].value;
	}


	// Return
	std::shared_ptr<ts::Node<T>> nodePtr (
		new ts::VertCatNode<T>(
			{res.rows(), res.cols()},
			dependencies,
			heights
		)
	);

	return ts::Tensor<T>(res, x[0].wList, nodePtr);
}



// Flattening

template <typename T>
ts::FlatteningNode<T>::FlatteningNode(
	std::vector<long> shape,
	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> xVal, int xDep,
	std::vector<long> newSize
) {

	// FlatteningNode specific constructor to store the size of original matrix
	// (this allows us to easily rescale the flattened vector in grad
	// computation)

	// New tensor shape (vector)
	this->rows = shape[0];
	this->cols = shape[1];

	this->values =  {xVal};	// [da/dx]
	this->dependencies =  {xDep};

	// Original matrix size
	size = newSize;
}



template <typename T>
Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> ts::FlatteningNode<T>::incrementGradient(
	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> &childDerivative,
	unsigned &j
) {

	// Used in the  ts::Tensor::grad() method. Computes the increment of a derivative
	// for a matrix flattening.

	// childDerivative is a flattened vector. We need to convert it back to a
	// matrix with the dimensions of the original matrix.

	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic> mat;
	mat.setZero(size[0], size[1]);

	#pragma omp parallel for
	for(unsigned i=0; i<size[1]; i++) {
		for(unsigned j=0; j<size[0]; j++) {
			mat(j, i) = childDerivative(j * size[1] + i, 0);
		}
	}

	return mat;
}



template <typename T>
ts::Tensor<T> ts::flattening(const ts::Tensor<T> &x) {
	// Flattening operation to convert matrix to vector
	// A x matrix of size m*n becomes the following vector :
	// x(1,1), ..., x(1, n), x(2,1), ..., x(m, n)
	// (the resulting size is (m*n, 1)

	// The gradient will have to be computed for a scalar
	x.wList->elementWiseOnly = false;


	// Set res vector
	Eigen::Array<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> res = x.value;
	res = Eigen::Map<Eigen::Array<T, -1, 1>>(
		res.data(), res.cols() * res.rows()
	);


	// Set dx matrix
	// It should be 1-filled since we're keeping all values of x in res, but
	// storing the full matrix would not be memory-efficient
	Eigen::Array<T, 0, 0> dx;


	// Return
	std::shared_ptr<ts::Node<T>> nodePtr (
		new ts::FlatteningNode<T>(
			{res.rows(), res.cols()},
			dx, x.index,
			{x.value.rows(), x.value.cols()}
		)
	);

	return ts::Tensor<T>(res, x.wList, nodePtr);
}
