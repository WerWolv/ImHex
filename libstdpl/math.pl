namespace std::math {

	fn min_u(u128 a, u128 b) {
		return (a < b) ? a : b;
	};
	
	fn min_i(s128 a, s128 b) {
		return (a < b) ? a : b;
	};
	
	fn min_f(double a, double b) {
		return (a < b) ? a : b;
	};
	
	
	fn max_u(u128 a, u128 b) {
		return (a > b) ? a : b;
	};
	
	fn max_i(s128 a, s128 b) {
		return (a > b) ? a : b;
	};
	
	fn max_f(double a, double b) {
		return (a > b) ? a : b;
	};
	
	
	fn abs_i(s128 value) {
		return value < 0 ? -value : value;
	};
	
	fn abs_d(double value) {
		return value < 0 ? -value : value;
	};
	
	
	fn ceil(double value) {
		s128 cast;
		cast = value;
		
		return cast + 1;
	};

}

std::print("{}", std::math::ceil(123.6));