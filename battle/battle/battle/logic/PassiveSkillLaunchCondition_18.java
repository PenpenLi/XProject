package com.nucleus.logic.core.modules.battle.logic;

import com.nucleus.commons.annotation.GenIgnored;
import com.nucleus.logic.core.modules.battle.model.BattleSoldier;
import com.nucleus.logic.core.modules.battle.model.CommandContext;

/**
 * 每隔N回合
 * 
 * @author wgy
 *
 */
@GenIgnored
public class PassiveSkillLaunchCondition_18 extends AbstractPassiveSkillLaunchCondition {
	private int round;

	@Override
	public boolean launchable(BattleSoldier soldier, BattleSoldier target, CommandContext context, IPassiveSkill passiveSkill) {
		if (round <= 0)
			return false;
		int currentRound = soldier.battle().getCount();
		if (currentRound <= 0)
			return false;
		return currentRound % round == 0;
	}

	public int getRound() {
		return round;
	}

	public void setRound(int round) {
		this.round = round;
	}
}
