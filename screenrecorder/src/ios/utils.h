#ifndef utils_h
#define utils_h

#include <dmsdk/sdk.h>

@interface LuaTask : NSObject
@property(nonatomic) int callback;
@property(nonatomic,retain) NSDictionary *event;
@property(nonatomic) bool deleteRef;
@end

@interface LuaLightuserdata : NSObject
-(instancetype)init:(void*)pointer;
-(void*)getPointer;
@end

@interface Utils : NSObject

+(void)checkArgCount:(lua_State*)L count:(int)countExact;
+(void)checkArgCount:(lua_State*)L from:(int)countFrom to:(int)countTo;
+(int)newRef:(lua_State*)L index:(int)index;
+(void)deleteRefIfNotNil:(int)ref;
+(void)put:(NSMutableDictionary*)hastable key:(NSString*)key value:(NSObject*)value;
+(NSMutableDictionary*)newEvent:(NSString*)name;
+(void)dispatchEventNumber:(NSNumber*)listener event:(NSMutableDictionary*)event;
+(void)dispatchEventNumber:(NSNumber*)listener event:(NSMutableDictionary*)event deleteRef:(bool)deleteRef;
+(void)dispatchEvent:(int)listener event:(NSMutableDictionary*)event;
+(void)dispatchEvent:(int)listener event:(NSMutableDictionary*)event deleteRef:(bool)deleteRef;
+(void)setCFunctionAsField:(lua_State*)L name:(const char*)name function:(lua_CFunction)function;
+(void)setCClosureAsField:(lua_State*)L name:(const char*)name function:(lua_CFunction)function upvalue:(void*)upvalue;
+(void)pushValue:(lua_State*)L value:(NSObject*)object;
+(void)pushHashtable:(lua_State*)L hashtable:(NSDictionary*)hashtable;
+(void)executeTasks:(lua_State*)L;

@end

@interface Scheme : NSObject
@property(nonatomic, readonly) int LuaTypeNumeric;
@property(nonatomic, readonly) int LuaTypeByteArray;

-(void)string:(NSString*)path;
-(void)number:(NSString*)path;
-(void)boolean:(NSString*)path;
-(void)table:(NSString*)path;
-(void)function:(NSString*)path;
-(void)lightuserdata:(NSString*)path;
-(void)userdata:(NSString*)path;
-(void)numeric:(NSString*)path;
-(void)byteArray:(NSString*)path;
-(id)get:(NSString*)path;

@end

@interface Table : NSObject

-(id)init:(lua_State*)L index:(int)index;
-(void)parse:(Scheme*)scheme;
-(bool)getBoolean:(NSString*)path default:(bool)defaultValue;
-(NSNumber*)getBoolean:(NSString*)path;
-(NSString*)getString:(NSString*)path default:(NSString*)defaultValue;
-(NSString*)getString:(NSString*)path;
-(NSString*)getStringNotNull:(NSString*)path;
-(double)getDouble:(NSString*)path default:(double)defaultValue;
-(NSNumber*)getDouble:(NSString*)path;
-(double)getDoubleNotNull:(NSString*)path;
-(int)getInteger:(NSString*)path default:(int)defaultValue;
-(NSNumber*)getInteger:(NSString*)path;
-(int)getIntegerNotNull:(NSString*)path;
-(long)getLong:(NSString*)path default:(long)defaultValue;
-(NSNumber*)getLong:(NSString*)path;
-(long)getLongNotNull:(NSString*)path;
-(NSData*)getByteArray:(NSString*)path default:(NSData*)defaultValue;
-(NSData*)getByteArray:(NSString*)path;
-(NSData*)getByteArrayNotNull:(NSString*)path;
-(LuaLightuserdata*)getLightuserdata:(NSString*)path default:(LuaLightuserdata*)defaultValue;
-(LuaLightuserdata*)getLightuserdata:(NSString*)path;
-(LuaLightuserdata*)getLightuserdataNotNull:(NSString*)pat;
-(int)getFunction:(NSString*)path default:(int)defaultValue;
-(NSNumber*)getFunction:(NSString*)path;
-(NSDictionary*)getTable:(NSString*)path default:(NSDictionary*)defaultValue;
-(NSDictionary*)getTable:(NSString*)path;

@end

@protocol LuaPushable
-(void)push:(lua_State*)L;
@end

#endif
