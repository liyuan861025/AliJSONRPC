/*
 Copyright (C) 2009 Olivier Halligon. All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 
 * Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 
 * Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.
 
 * Neither the name of the author nor the names of its contributors may be used
 to endorse or promote products derived from this software without specific
 prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "JSONRPCResponseHandler.h"
#import "JSON.h"

#import "JSONRPCMethodCall.h"
#import "JSONRPCService.h"
#import "JSONRPC_Extensions.h"

//! @private Private API @internal
@interface JSONRPCResponseHandler()
-(id)objectFromJson:(id)jsonObject; //!< @private @internal
@end

@implementation JSONRPCResponseHandler
@synthesize methodCall = _methodCall;
@synthesize delegate = _delegate;
@synthesize callback = _callbackSelector;

@synthesize resultClass = _resultClass;

-(void)setDelegate:(id<JSONRPCDelegate>)aDelegate callback:(SEL)callback
{
	self.delegate = aDelegate;
	self.callback = callback;
	if (![aDelegate respondsToSelector:callback]) {
		NSLog(@"%@ warning: %@ does not respond to selector %@",[self class],aDelegate,NSStringFromSelector(callback));
	}
}
-(void)setDelegate:(id<JSONRPCDelegate>)aDelegate callback:(SEL)callback resultClass:(Class)cls {
	[self setDelegate:aDelegate callback:callback];
	self.resultClass = cls;
}

-(void)dealloc {
	[_methodCall release];
	[_delegate release];
	[super dealloc];
}


- (void)connection:(NSURLConnection *)connection didReceiveResponse:(NSURLResponse *)response
{
	[_receivedData release];
	_receivedData = [[NSMutableData alloc] init];
}
- (void)connection:(NSURLConnection *)connection didReceiveData:(NSData *)data
{
	[_receivedData appendData:data];
}


- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
	SEL errSel = @selector(methodCall:didFailWithError:);
	BOOL cont = YES;
	
	if (_delegate && [_delegate respondsToSelector:errSel]) {
		cont = [_delegate methodCall:self.methodCall didFailWithError:error];
	}
	if (cont)
	{
		id<JSONRPCDelegate> del = self.methodCall.service.delegate;
		if (del && [del respondsToSelector:errSel]) {
			cont = [del methodCall:self.methodCall didFailWithError:error];
		}
		(void)cont; // UNUSED AFTER THAT
	}
	
	[_receivedData release];
	_receivedData = nil;
}
- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
	NSError* jsonParsingError = nil;
	NSString* jsonStr = [[[NSString alloc] initWithData:_receivedData encoding:NSUTF8StringEncoding] autorelease];
	SBJSON* parser = [[SBJSON alloc] init];
	id respObj = [parser objectWithString:jsonStr error:&jsonParsingError];
	[parser release];

	[_receivedData release];
	_receivedData = nil;

	if (jsonParsingError) {
		// raise an error regarding JSON Parsing
		NSMutableDictionary* verboseUserInfo = [NSMutableDictionary dictionaryWithDictionary:[jsonParsingError userInfo]];
		[verboseUserInfo setObject:jsonStr forKey:JSONRPCErrorJSONObjectKey];
		NSError* verboseJsonParsingError = [NSError errorWithDomain:[jsonParsingError domain]
															   code:[jsonParsingError code]
														   userInfo:verboseUserInfo];
		
		SEL errSel = @selector(methodCall:didFailWithError:);
		BOOL cont = YES;
		if (_delegate && [_delegate respondsToSelector:errSel]) {
			cont = [_delegate methodCall:self.methodCall didFailWithError:verboseJsonParsingError];
		}
		if (cont)
		{
			id<JSONRPCDelegate> del = self.methodCall.service.delegate;
			if (del && [del respondsToSelector:errSel]) {
				cont = [del methodCall:self.methodCall didFailWithError:verboseJsonParsingError];
			}
			(void)cont; // UNUSED AFTER THAT
		}			
	} else {
		// extract result from JSON response
		id resultJsonObject = [respObj objectForKey:@"result"];
		if (resultJsonObject == [NSNull null]) resultJsonObject = nil;
		id parsedResult = resultJsonObject;
		if (resultJsonObject && _resultClass) {
			// decode object as expected Class
			parsedResult = [self objectFromJson:resultJsonObject];
			if (!parsedResult) {
				// raise and error regarding conversion (send to _delegate or fallback to service)
				SEL errSel = @selector(methodCall:didFailToConvertResponse:toClass:);
				NSDictionary* userInfo = [NSDictionary dictionaryWithObjectsAndKeys:
										  resultJsonObject,JSONRPCErrorJSONObjectKey,
										  NSLocalizedString(@"error.json-conversion.message","Error message upon conversion"),NSLocalizedDescriptionKey,
										  NSStringFromClass(_resultClass),JSONRPCErrorClassNameKey,
										  nil];
				NSError* error = [NSError errorWithDomain:JSONRPCInternalErrorDomain
													 code:JSONRPCConversionErrorCode
												 userInfo:userInfo];
				BOOL cont = YES;
				if (_delegate && [_delegate respondsToSelector:errSel]) {
					cont = [_delegate methodCall:self.methodCall didFailWithError:error];
				}
				if (cont)
				{
					id<JSONRPCDelegate> del = self.methodCall.service.delegate;
					if (del && [del respondsToSelector:errSel]) {
						cont = [del methodCall:self.methodCall didFailWithError:error];
					}
					(void)cont; // UNUSED AFTER THAT
				}
				return;
			}
		}
		
		// extract error from JSON response
		id errorJsonObject  = [respObj objectForKey:@"error"];
		if (errorJsonObject == [NSNull null]) errorJsonObject = nil;
		NSError* parsedError = nil;
		if (errorJsonObject) {
			parsedError = [NSError errorWithDomain:JSONRPCServerErrorDomain
											  code:[[errorJsonObject objectForKey:@"code"] longValue]
										  userInfo:[NSDictionary dictionaryWithObjectsAndKeys:
													[errorJsonObject objectForKey:@"message"]?:@"",NSLocalizedDescriptionKey,
													errorJsonObject,JSONRPCErrorJSONObjectKey,
													nil]];
		}

		JSONRPCMethodCall* methCall = self.methodCall;
		NSObject<JSONRPCDelegate>* realDelegate = _delegate ?: methCall.service.delegate;
		SEL realSel = _callbackSelector ?: @selector(methodCall:didReturn:error:);

		if (realDelegate) {
			NSInvocation* inv = [NSInvocation invocationWithMethodSignature:[realDelegate methodSignatureForSelector:realSel]];
			[inv setSelector:realSel];
			[inv setArgument:&methCall atIndex:2];
			[inv setArgument:&parsedResult atIndex:3];
			[inv setArgument:&parsedError atIndex:4];
			[inv invokeWithTarget:realDelegate];
		} else {
			NSLog(@"warning: JSONRPCResponseHandler did receive a response but no delegate defined: the response has been ignored"); 
		}
	}	
}

-(id)objectFromJson:(id)jsonObject {
	//if ([_resultConvertionClass instancesRespondToSelector:@selector(initWithJson:)])
	{
		if ([jsonObject isKindOfClass:[NSArray class]]) {
			// Convert each object in the NSArray
			return [NSArray arrayWithJson:jsonObject itemsClass:_resultClass];
		} else {
			// not an NSArray
			return [[[_resultClass alloc] initWithJson:jsonObject] autorelease];
		}
	}
}
@end



