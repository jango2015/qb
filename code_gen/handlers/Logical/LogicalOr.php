<?php

// NOTE: employed only in Pixel Bender programs 

class LogicalOr extends Handler {

	use ScalarAddressMode, BinaryOperator, ScalarResult, BooleanResult, BooleanOnly;

	protected function getActionOnUnitData() {
		return "res = op1 || op2;";
	}
}

?>