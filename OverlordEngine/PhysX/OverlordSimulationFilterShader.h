#pragma once
#define FILTERSHADERTYPE_CUSTOM

inline PxFilterFlags OverlordSimulationFilterShader(
	PxFilterObjectAttributes attribute0, PxFilterData filterData0,
	PxFilterObjectAttributes attribute1, PxFilterData filterData1,
	PxPairFlags& pairFlags, const void* pConstantBlock, PxU32 blockSize)
{
	PX_UNUSED(pConstantBlock);
	PX_UNUSED(blockSize);

	//if (filterData0.word3 == 0 || filterData1.word3 == 0)
	//{
	//	return physx::PxDefaultSimulationFilterShader(attribute0, filterData0, attribute1, filterData1, pairFlags, pConstantBlock, blockSize);
	//	//return vehicle::VehicleFilterShader(attribute0, filterData0, attribute1, filterData1, pairFlags, pConstantBlock, blockSize);
	//}

	if ((0 == (filterData0.word0 & filterData1.word1)) && (0 == (filterData1.word0 & filterData0.word1)))
		return PxFilterFlag::eSUPPRESS;

	if (((attribute0 & PxFilterObjectFlag::eTRIGGER) != 0 || (attribute1 & PxFilterObjectFlag::eTRIGGER) != 0)
		&& (0 == (filterData0.word0 & vehicle::COLLISION_FLAG_WHEEL)) // IGNORE THE WHEELS WHEN PASSING CHECKPOINT
		&& (0 == (filterData1.word0 & vehicle::COLLISION_FLAG_WHEEL)))
	{
		//pairFlags |= PxPairFlag::eNOTIFY_TOUCH_LOST;
		pairFlags |= PxPairFlag::eTRIGGER_DEFAULT;
		return PxFilterFlag::eDEFAULT;
	}

	if((filterData0.word0 & filterData1.word0) != 0)
	{
		pairFlags |= PxPairFlag::eNOTIFY_TOUCH_FOUND;
	}

	pairFlags |= PxPairFlag::eCONTACT_DEFAULT;
	return PxFilterFlag::eDEFAULT;
}

//PxFilterFlags DefaultSimulationFilter(
//	PxFilterObjectAttributes attribute0, PxFilterData filterData0,
//	PxFilterObjectAttributes attribute1, PxFilterData filterData1,
//	PxPairFlags& pairFlags, const void* , PxU32 )
//{
//	// let triggers through
//	if (PxFilterObjectIsTrigger(attribute0) || PxFilterObjectIsTrigger(attribute1))
//	{
//		pairFlags = PxPairFlag::eTRIGGER_DEFAULT;
//		return PxFilterFlag::eDEFAULT;
//	}
//	// generate contacts for all that were not filtered above
//	pairFlags = PxPairFlag::eCONTACT_DEFAULT;
//
//	// trigger the contact callback for pairs (A,B) where
//	// the filtermask of A contains the ID of B and vice versa.
//	if ((filterData0.word0 & filterData1.word1) && (filterData1.word0 & filterData0.word1))
//		pairFlags |= PxPairFlag::eNOTIFY_TOUCH_FOUND;
//
//	return PxFilterFlag::eDEFAULT;
//}


