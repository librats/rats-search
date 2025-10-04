function biasValuation(count, min, avrg, avrg_const)
{
	return ((count / (count + min)) * avrg) + ((min / (min + count)) * avrg_const);
}

function rating(good, bad){
	if (good + bad > 0)
	{
		return biasValuation(good + bad, 9, good / (good + bad), 0.45);
	}
	else
	{
		return 0;
	}
}

module.exports = rating;
