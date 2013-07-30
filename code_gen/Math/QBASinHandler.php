<?php

class QBASinHandler extends QBHandler {

	protected function getScalarExpression() {
		$type = $this->getOperandType(2);
		$cType = $this->getOperandCType(2);
		$f = ($type == 'F32') ? 'f' : '';
		return "res = asin$f(op1);";
	}
}

?>