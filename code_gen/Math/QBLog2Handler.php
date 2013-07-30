<?php

class QBLog2Handler extends QBHandler {

	protected function getScalarExpression() {
		$type = $this->getOperandType(2);
		$cType = $this->getOperandCType(2);
		$f = ($type == 'F32') ? 'f' : '';
		return "res = log2$f(op1);";
	}
}

?>