/*
 * act123.cpp
 * Contains definitions for checking actions 1-3.
 *
 * Copyright 2004-2009 by Dale McCoy.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include<string>
#include<fstream>
#include<cassert>
#include<errno.h>
#include<cstdlib>

using namespace std;

#include"inlines.h"
#include"ExpandingArray.h"
#include"sanity_defines.h"
#include"data.h"
#include"rangedint.h"
#include"pseudo.h"
#include"act123.h"
#include"command.h"
#include"messages.h"

#define MAX_TTD_SPRITE 4894

uint CargoTransTable(int=0);

#define CHANGED_FEATURE(type)\
	{\
		int loop;\
		loop=0;\
type##ChangedFeature:\
		if(loop++>10){\
			IssueMessage(FATAL,AUTOCORRECT_LOOP);\
			return;\
		}\
	}

/*int parity(uint x){
	int ret=0;
	while(x){
		ret+=x%2;
		x/=2;
	}
	return ret;
}*/


bool CheckSpriteNum(uint num,uint offset,act123::Act1&act1,uint feature,bool&mismatch,bool&hasGround){
	uint sprite=num&0x3FFF;
	if(num&1<<31){
		if(feature!=act1.feature&&!mismatch){
			IssueMessage(ERROR,FEATURE_MISMATCH,1,act1.spritenum);
			mismatch=true;
			return false;
		}
		if(sprite>=act1.numsets)
			IssueMessage(ERROR,UNDEFINED_SPRITE_SET,offset,sprite,act1.spritenum);
		else act1.use(sprite);
	}else if(sprite>MAX_TTD_SPRITE)
		IssueMessage(ERROR,SPRITENUM_TOO_HIGH,offset);
	hasGround|=((num&1<<30)!=0);
	int translate=num>>14&3,color=num>>16&0x3FFF;
	if(translate==3)IssueMessage(ERROR,INVALID_COLOR_TRANS,offset);
	else if((translate==1||(translate==2&&color))&&(color<775||color>790)&&(color<795||color>803))
		IssueMessage(WARNING1,INVALID_COLOR_SPRITE,offset+2,color);
	return true;
}

bool CheckCargoID(uint offset,unsigned int id,uint feature,uint&newfeature){
	act123::IDarray&IDs=act123::Instance().defined2IDs;
	if(id&0x8000)return false;//callback
	else if(id>>8){
		IssueMessage(ERROR,NEITHER_ID_CALLBACK,id,CID);
		return true;
	}
	if(IDs.test(offset,id)){
		if(IDs.GetFeature(id)!=feature){
			IssueMessage(ERROR,FEATURE_LINK_MISMATCH,offset,id,IDs.GetFeature(id));
			if(newfeature==(uint)-1)newfeature=IDs.GetFeature(id);
			else if(newfeature!=IDs.GetFeature(id))newfeature=feature;
		}else newfeature=feature;
		IDs.use(id);
		return IDs.checks1C(id);
	}else
		return true;
}

void invalidate_act3(){
	act123::Instance().act3spritenum=0;
}

void CheckCallback(uint offs,uint feature,uint cb){
	Callbacks&cbs=Callbacks::Instance();
	if(cb&&(cb>=cbs.numcallbacks||cb<0x10||(cbs[cb]&0x80000000?!(cbs[cb]&(1<<feature)):feature!=cbs[cb])))
		IssueMessage(ERROR,INVALID_CALLBACK,offs,cb);
}

int Check1(PseudoSprite&data){
	act123::Act1&act1=act123::Instance().act1;
	const uint length=data.Length();
	if(act1.spritenum)
		for(unsigned int i=0;i<act1.numsets;i++)
			if(!act1.is_used(i))IssueMessage(WARNING1,UNUSED_SET,i,act1.spritenum);
	act1.init();
	if(CheckLength(length,(data.ExtractByte(3)==0xFF)?6:4,INVALID_LENGTH,ACTION,1,ONE_OF,4,6))return 0;
	if(!IsValidFeature(ACT1|EMPTY1,act1.feature=data.ExtractByte(1)))IssueMessage(ERROR,INVALID_FEATURE);
	act1.numsets=data.ExtractByte(2);
	if(!act1.numsets)IssueMessage(WARNING1,NO_SETS,1);
	int numsprites=data.ExtractExtended(3);
	if(!numsprites&&!IsValidFeature(EMPTY1,act1.feature))IssueMessage(WARNING1,NO_SPRITES,1);
	if(numsprites&&!IsValidFeature(ACT1,act1.feature)&&IsValidFeature(EMPTY1,act1.feature))IssueMessage(ERROR,INVALID_FEATURE);
	if((numsprites>4&&(act1.feature==7||act1.feature==9))||(numsprites>8&&act1.feature<4)||
		(numsprites>1&&act1.feature==0x0B))IssueMessage(WARNING1,SET_TOO_LARGE);
	else if(numsprites>4&&numsprites<8&&act1.feature<4)IssueMessage(WARNING1,STRANGE_SET_SIZE);
	act1.spritenum=_spritenum;
	return numsprites*act1.numsets;
}

void Check2(PseudoSprite&data){
	data.SetAllHex();
	act123::Act1&act1=act123::Instance().act1;
	uint feature=data.ExtractByte(1),id=data.ExtractByte(2);
	uint nument1=data.ExtractByte(3),nument2=data.ExtractByte(4),length=data.Length(),i,j;
	Define2 defineID(feature,id);
	if(!IsValid2Feature(feature)){
		IssueMessage((nument1&0x80)?ERROR:FATAL,INVALID_FEATURE);
		if(_autocorrect==2&&act1.spritenum&&!(nument1&0x80)){
			IssueMessage(0,CONSOLE_AUTOCORRECT,_spritenum);
			IssueMessage(0,AUTOCORRECTING,1,FEATURE,feature,act1.feature);
			data.SetByteAt(1,feature=act1.feature);
		}else if(!(nument1&0x80)) return;
	}
	switch(nument1){
	case 0x80:
	case 0x83:
	case 0x84:
	{
CHANGED_FEATURE(rand)
		bool isRand=false;
		unsigned int prevID=(unsigned)-1,rID,newfeature=(uint)-1;
		uint base = 7;	// offset of first randID
		if(nument1==0x84){
			base = 8;
			if(data.ExtractByte(4)&0x30)
				IssueMessage(ERROR,BAD_RANDSUBTYPE);
		}
		nument2=data.ExtractByte(base-1);
		if((nument2-1)&nument2/*tests for only one bit set*/||!nument2)IssueMessage(ERROR,RAND_2_NUMSETS);
		else if(nument2==1)IssueMessage(WARNING3,ONLY_ONE_CHOICE);
		rand2::Instance().CheckRand(feature,nument1,data.ExtractByte(base-3),data.ExtractByte(base-2),nument2);
		if(_autocorrect&&!((length-base)%2)){
			uint realcount=(length-base)/2;
			if(nument2!=realcount && realcount<256 && !((realcount-1)&realcount)){
				IssueMessage(0,CONSOLE_AUTOCORRECT,_spritenum);
				IssueMessage(0,AUTOCORRECTING,base-1,NRAND,nument2,realcount);
				data.SetByteAt(base-1,nument2=realcount);
			}
		}
		if(CheckLength(length,base+2*nument2,BAD_LENGTH,NRAND,VAL,nument2,base+2*nument2))return;
		for(i=0;i<nument2;i++){
			rID=data.SetNoEol(base+2*i).ExtractWord(base+2*i);
			CheckCargoID(base+i*2,rID,feature,newfeature);
			if(prevID!=(unsigned)-1)
				isRand|=(prevID!=rID);
			prevID=rID;
		}
		if(!isRand&&nument2!=1)
			IssueMessage(WARNING3,NOT_RANDOM);
		if(_autocorrect&&newfeature!=(uint)-1&&newfeature!=feature){
			IssueMessage(0,CONSOLE_AUTOCORRECT,_spritenum);
			IssueMessage(0,AUTOCORRECTING,1,FEATURE,feature,newfeature);
			data.SetByteAt(1,feature=newfeature);
			defineID.ChangeFeature(feature);
			goto randChangedFeature;
		}
		return;
	}
	case 0x81:case 0x82:
	case 0x85:case 0x86:
	case 0x89:case 0x8A:
	{
#define Is60x(var) (((var)&0xE0)==0x60)
CHANGED_FEATURE(var)
		uint extract=1<<((nument1>>2)&3),off=4,var,param=0,shift,op=(uint)-1,newfeature=(uint)-1,
			effFeature = Check2v::GetEffFeature(feature,nument1);
		bool isvar=false,isadv=false;
		uint oldop = 0;
		varRange ranges(extract);
		while(true){//read <var> [<param>] <varadjust> [<op> ...]. off reports byte to be read.
			if(Is60x(var=data.ExtractByte(off++)))param=data.ExtractByte(off++);
			shift=data.ExtractByte(off++);
			Check2v::Instance().Check(effFeature,var,off-2-(Is60x(var)?1:0),param,shift&0x1F);
			if((shift&0xC0)==0xC0){
				IssueMessage(FATAL,INVALID_SHIFT,off-1);
				return;
			}
			if(isadv||(shift&0x20))data.SetEol(off-(Is60x(var)?4:3),2);
			ranges.UpdateRange(var,op,shift,data,off);
			defineID.Check(var);
			isvar|=(var!=0x1A&&var!=0x1C);
			if(!(shift&0x20))break;
			isadv=true;
			if((op=data.ExtractByte(off++))>Check2v::GetMaxOp())
				IssueMessage(ERROR,INVALID_OP,off-1,op);
			else
				data.SetOpByte(off-1, '2');
			if(op==0xF && oldop!=0xE && oldop!=0x10)
				IssueMessage(WARNING1,DISCARD_UNSTORED,off-1);
			else if(op==0x10 && !Check2v::Instance().IsValid(effFeature, 0x7C))
				IssueMessage(ERROR,NO_PERS_REGS,off-1);
			oldop = op;
		}
		nument2=data.ExtractByte(off);//off switches to byte-just-read.
		if(isadv)data.SetEol(off-1,1);
		else if(nument2>1)data.SetEol(off,1);
		if(!isvar)IssueMessage(WARNING4,NOT_VARIATIONAL);
		uint width=2+extract*2,end=++off+width*nument2;
		int change=int(length-end-2)/int(width);
		if(change&&_autocorrect&&!((length-end-2)%width)&&nument2+change<256){
			IssueMessage(0,CONSOLE_AUTOCORRECT,_spritenum);
			IssueMessage(0,AUTOCORRECTING,off-1,NVAR,nument2,nument2+change);
			data.SetByteAt(off-1,nument2+=change);
			end=length-2;
		}
		if(CheckLength(length,end+2,BAD_LENGTH,NVAR,VAL,nument2,end+2))return;
		uint def=data.ExtractWord(end),vID;
		for(i=off;i<end;i+=width){//read <ID> <min> <max> [...]
			vID=data.ExtractWord(i);
			isvar=CheckCargoID(i,vID,feature,newfeature);
			uint min=data.ExtractVariable(i+2,extract),max=data.ExtractVariable(i+2+extract,extract);
			if(nument2>1)data.SetEol(i+1+extract*2,isadv?2:1);
			if(min>max)IssueMessage((i==off)?WARNING4:WARNING1,UNREACHABLE_VAR,(i-off)/width);
			else{
				ranges.AddRange(min,max);
				if(def==vID&&(nument2!=1||!isvar))IssueMessage(WARNING3,REUSED_DEFAULT);
			}
			if(var==0x0C&&!isadv){
				if(min!=max&&!(
					(feature==7&&min==0x1B&&max==0x1C)||
					(feature==0&&min==0x10&&max==0x11&&(vID==0x8000||vID==0xFF00))
					))
					IssueMessage(WARNING1,CHECK_0C_RANGE,i+2,min,max);
				else{
					CheckCallback(i+2,feature,min);
				}
			}
		}
		ranges.CheckDefault();
		CheckCargoID(end,def,feature,newfeature);
		if(_autocorrect&&newfeature!=(uint)-1&&newfeature!=feature){
			IssueMessage(0,CONSOLE_AUTOCORRECT,_spritenum);
			IssueMessage(0,AUTOCORRECTING,1,FEATURE,feature,newfeature);
			data.SetByteAt(1,feature=newfeature);
			defineID.ChangeFeature(feature);
			goto varChangedFeature;
		}
		return;
	}
	default:
CHANGED_FEATURE(std)
		if(nument1&0x80){
			IssueMessage(FATAL,INVALID_TYPE);
			return;
		}
		switch(Get2Type(feature)){
		case 0:
		case 1:
		case 2:{ // Standard format
			bool mismatch=false,no1=false;
			if(CheckLength(length,2*(nument1+nument2)+5,BAD_LENGTH,VARS,NUMENT1,NUMENT2,VALS,nument1,nument2,
						   2*(nument1+nument2)+5))
				break;
			if (Get2Type(feature) == 0) {
				if(!nument1)IssueMessage(ERROR,NO_REQD_SETS,LOADED);
				if(!nument2)IssueMessage(ERROR,NO_REQD_SETS,LOADING);
			} else if (Get2Type(feature) == 1) {
				if(!nument2)IssueMessage(ERROR,NO_REQD_SETS,LOTS);
			} else {
				if(nument1!=1)IssueMessage(ERROR,INVALID_LITERAL,3,nument1,1);
				if(nument2)IssueMessage(ERROR,INVALID_LITERAL,4,nument2,0);
			}
			for(i=0;i<nument1+nument2;i++){
				j=data.ExtractWord(2*i+5);
				if(j&0x8000);//Callback
				else if(j>>8)IssueMessage(ERROR,NEITHER_ID_CALLBACK,j,SET);
				else if(act1.spritenum==0&&!no1){
					IssueMessage(ERROR,NO_ACT1);
					no1=true;
				}else if(feature!=act1.feature&&!mismatch&&!no1){
					mismatch=true;
					IssueMessage(ERROR,FEATURE_MISMATCH,1,act1.spritenum);
					if(_autocorrect==2||(_autocorrect&&Get2Type(act1.feature)<3)){
						IssueMessage(0,CONSOLE_AUTOCORRECT,_spritenum);
						IssueMessage(0,AUTOCORRECTING,1,FEATURE,feature,act1.feature);
						data.SetByteAt(1,feature=act1.feature);
						defineID.ChangeFeature(feature);
						goto stdChangedFeature;
					}
				}else if(j>=act1.numsets)
					IssueMessage(ERROR,UNDEFINED_SPRITE_SET,2*i+5,j,act1.spritenum);
				else act1.use(j);
			}
			break;
		}case 3:{ // House/Industry tile format
			uint ground=data.ExtractDword(4);
			bool mismatch=false,hasGround=(ground!=0);
			if(ground)CheckSpriteNum(ground,4,act1,feature,mismatch,hasGround);
			if(nument1){//Extended format
				data.SetEol(3,3);//EOL before ground sprite
				data.SetEol(7,1);//EOL after ground sprite
				uint off=7;
				for(i=0;i<nument1||_autocorrect;/*increment in last statement in try block*/){
					try{
						uint building=data.ExtractDword(off+1),xoff=data.ExtractByte(off+5),
							yoff=data.ExtractByte(off+6),zoff=data.ExtractByte(off+7);
						if(!CheckSpriteNum(building,off+1,act123::Instance().act1,feature,mismatch,hasGround)&&
							(_autocorrect==2||(_autocorrect&&Get2Type(act1.feature)==3))){
							IssueMessage(0,CONSOLE_AUTOCORRECT,_spritenum);
							IssueMessage(0,AUTOCORRECTING,1,FEATURE,feature,act1.feature);
							data.SetByteAt(1,feature=act1.feature);
							goto stdChangedFeature;
						}
						if(!building)IssueMessage(ERROR,NO_BUILDING_SPRITE,off+1);
						if(zoff!=0x80){
							uint x=data.ExtractByte(off+8),y=data.ExtractByte(off+9);
							data.ExtractByte(off+10);
							off+=3;
							if(xoff>15)IssueMessage(WARNING3,TOO_LARGE,off+2,XOFF,15);
							else if(xoff+x>16)IssueMessage(WARNING3,TOO_LARGE,off+2,XOFF_EXT,16);
							if(yoff>15)IssueMessage(WARNING3,TOO_LARGE,off+3,YOFF,15);
							else if(yoff+y>16)IssueMessage(WARNING3,TOO_LARGE,off+3,YOFF_EXT,16);
							//if(zoff+z>0x87)IssueMessage(WARNING1,TOO_LARGE,off+4,ZOFF_EXT,0x87);
						}else if(i==0)IssueMessage(ERROR,FIRST_SPRITE_CANNOT_SHARE);
						off+=7;
						if(++i!=nument1)data.SetEol(off,2);
						if(i==0x7F&&_autocorrect)throw 0;
					}catch(...){
						if(_autocorrect&&i){
							if(i!=nument1){
								IssueMessage(0,CONSOLE_AUTOCORRECT,_spritenum);
								IssueMessage(0,AUTOCORRECTING,3,NUMSPRITES,nument1,i);
								data.SetByteAt(3,i);
							}
							break;
						}else{
							IssueMessage(FATAL,OVERRAN_SPRITE,i);
							return;
						}
					}
				}
				if(++off!=length)IssueMessage(WARNING2,EXTRA_DATA,off);
				if(!hasGround)IssueMessage(WARNING2,NO_GROUNDSPRITE,NONTRANS);
			}else{//Basic format
				if(!ground)IssueMessage(ERROR,NO_GROUNDSPRITE,GROUND);
				if(CheckLength(length,17,INVALID_LENGTH,TYPE,BASICSTD2,HOUSE_INSTYTILE,EXACTLY,17))break;
				uint building=data.ExtractDword(8),xoff=data.ExtractByte(12),yoff=data.ExtractByte(13),
					x=data.ExtractByte(14),y=data.ExtractByte(15);//,z=data.ExtractByte(16);
				if(building){
					if(!CheckSpriteNum(building,8,act1,feature,mismatch,hasGround)&&
						(_autocorrect==2||(_autocorrect&&Get2Type(act1.feature)==3))){
						IssueMessage(0,CONSOLE_AUTOCORRECT,_spritenum);
						IssueMessage(0,AUTOCORRECTING,1,FEATURE,feature,act1.feature);
						data.SetByteAt(1,feature=act1.feature);
						goto stdChangedFeature;
					}
					if(xoff>15)IssueMessage(WARNING3,TOO_LARGE,12,XOFF,15);
					else if(xoff+x>16)IssueMessage(WARNING3,TOO_LARGE,12,XOFF_EXT,16);
					if(yoff>15)IssueMessage(WARNING3,TOO_LARGE,13,YOFF,15);
					else if(yoff+y>16)IssueMessage(WARNING3,TOO_LARGE,13,YOFF_EXT,16);
					//if(z>0x87)IssueMessage(WARNING1,TOO_LARGE,16,ZEXT,0x87);
				}
			}
			break;
		}case 4: // Industry format
			switch(data.ExtractByte(3)){ // Callback version
			case 0:
				if(CheckLength(length,15,INVALID_LENGTH,TYPE,PROD2S,INDUSTRIES,ONE_OF,10,15))break;
				if(data.ExtractWord(10)>32768)IssueMessage(WARNING1,EXCESSIVE_ADD,1);
				if(data.ExtractWord(12)>32768)IssueMessage(WARNING1,EXCESSIVE_ADD,2);
				break;
			case 1:
				CheckLength(length,10,INVALID_LENGTH,TYPE,PROD2S,INDUSTRIES,ONE_OF,10,15);
				break;
			default:
				IssueMessage(ERROR,INVALID_VERSION,PROD2);
			}
			break;
		DEFAULT(feature)
		}
	}
}

void Check3(PseudoSprite&data){
	PseudoSprite::Byte feature, numIDs;
CHANGED_FEATURE(act3)
	data.seek(1)>>feature>>numIDs;
	uint newfeature=(uint)-1, i;
	bool isOverride=((numIDs&0x80)!=0),isGeneric=((numIDs&0x7F)==0);
	if(isOverride&&isGeneric)IssueMessage(ERROR,GENERIC_AND_OVERRIDE);
	if(!isGeneric&&!IsValidFeature(ACT3,feature)){IssueMessage(FATAL,INVALID_FEATURE);return;}
	else if(isOverride&&!IsValidFeature(OVERRIDE3,feature))IssueMessage(ERROR,INVALID_FEATURE);
	else if(isGeneric&&!IsValidFeature(GENERIC3,feature))IssueMessage(ERROR,INVALID_FEATURE);
	Expanding0Array<int> ids;
	PseudoSprite::ExtByte id;
	for(i=0;i<(numIDs&0x7F);i++){
		data>>id;
		if(ids[id])
			IssueMessage(WARNING1,DUPLICATE,id.loc(),ID,id.val(),ids[id]);
		ids[id]=id.loc();
		CheckID(feature,id);
		if(!IsValidFeature(ACT3_BEFORE_PROP08,feature) && !IsProp08Set(feature,id))
			IssueMessage(ERROR,ACT3_PRECEDES_PROP08,id.loc(),id.val());
	}

	PseudoSprite::Byte numCIDs;
	data>>numCIDs;
	uint newCIDs=data.BytesRemaining()/3;
	if(_autocorrect>=2&&(data.BytesRemaining()%3)==2&&newCIDs!=numCIDs&&newCIDs<256){
		IssueMessage(0,CONSOLE_AUTOCORRECT,_spritenum);
		IssueMessage(0,AUTOCORRECTING,numCIDs.loc(),NUMCID,numCIDs.val(),newCIDs);
		numCIDs.set(newCIDs);
	}

	if(numCIDs&&feature>4)IssueMessage(WARNING1,NO_CARGOTYPES);

	PseudoSprite::Byte cargo;
	PseudoSprite::Word cid,def;
	data.Extract(def,numCIDs.loc()+1+numCIDs*3);

	ids.clear();	// Reuse as cargos
	ids.reserve(256);
	for (i=0; i<numCIDs; i++) {
		data>>cargo>>cid;
        if(cargo>CargoTransTable() && cargo!=0xFF && (cargo!=0xFE||feature!=4))
			IssueMessage(ERROR,INVALID_CARGO_TYPE,cargo.loc(),cargo.val());
		if(ids[cargo])
			IssueMessage(WARNING1,DUPLICATE,cargo.loc(),CARGO,cargo.val(),ids[cargo]);
		ids[cargo] = cargo.loc();
		CheckCargoID(cid.loc(),cid,feature,newfeature);
		if(def==cid)
			IssueMessage(WARNING1,REUSED_DEFAULT);
	}

	CheckCargoID(def.loc(),def,feature,newfeature);

	if (isOverride && act123::Instance().act3spritenum && feature != act123::Instance().act3feature) {
		IssueMessage(ERROR,FEATURE_MISMATCH, 3, act123::Instance().act3spritenum);
		newfeature = act123::Instance().act3feature;
	}

	if(_autocorrect&&newfeature!=(uint)-1&&newfeature!=feature){
		IssueMessage(0,CONSOLE_AUTOCORRECT,_spritenum);
		IssueMessage(0,AUTOCORRECTING,1,FEATURE,feature.val(),newfeature);
		feature.set(newfeature);
		goto act3ChangedFeature;
	}

	if(isGeneric)// Generic 3s cannot be followed by an override
		act123::Instance().act3spritenum=0;
	else if(!isOverride){
		act123::Instance().act3feature=feature;
		act123::Instance().act3spritenum=_spritenum;
	}else if(!act123::Instance().act3spritenum) IssueMessage(ERROR,NO_STD_3);
}

void Init123(){act123::Instance().init();}

void final123(){
	const act123::IDarray& IDs=act123::CInstance().defined2IDs;
	ManualConsoleMessages();
	bool header=false;
	if(act123::Instance().act1.spritenum)
		for(unsigned int i=0;i<act123::Instance().act1.numsets;i++)
			if(!act123::CInstance().act1.is_used(i)){
				IssueMessage(WARNING1,UNUSED_SET,i,act123::Instance().act1.spritenum);
				if(!header){
					IssueMessage(WARNING1,UNEXP_EOF_STD2);
					header=true;
				}
			}
	int j;
	for(uint i=0;i<=act123::Instance().MaxFoundFeat();i++){
		header=false;
		for(j=0;j<256;j++)
			if(IDs.GetFeature(j)==i&&IDs.is_defined(j)&&!IDs.is_used(j)){
				if(!header){
					IssueMessage(WARNING1,UNUSED2IDLEAD,i);
					IssueMessage(WARNING1,UNEXP_EOF_CARGOID,i);
					header=true;
				}
				IssueMessage(WARNING1,UNUSEDIDFINAL,j,IDs.defined_at(j));
			}
	}
}
