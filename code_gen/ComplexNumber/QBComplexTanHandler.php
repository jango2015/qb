<?php

class QBComplexTanHandler extends QBComplexNumberHandler {

	public function getHelperFunctions() {
		$type = $this->getOperandType(2);
		$cType = $this->getOperandCType(2);
		$f = ($type == 'F32') ? 'f' : '';
		$functions = array(
			array(
				"void ZEND_FASTCALL qb_calculate_complex_tan_$type(qb_complex_$type *z, qb_complex_$type *res) {",
					"$cType w = 1 / (cos$f(2.0 * z->r) + cosh$f(2.0 * z->i));",
					"$cType r = w * sin$f(2.0 * z->r);",
					"$cType i = w * sinh$f(2.0 * z->i);",
					"res->r = r; res->i = i;",
				"}",
			),
		);
		return $functions;
	}
	
	protected function getComplexNumberExpression() {
		$type = $this->getOperandType(1);
		return "qb_calculate_complex_tan_$type((qb_complex_$type *) op1_ptr, (qb_complex_$type *) res_ptr);";
	}
}

?>