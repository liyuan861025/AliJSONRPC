//
//  JSONRPC.h
//  JSONRPC
//
//  Created by Olivier on 01/05/10.
//  Copyright 2010 AliSoftware. All rights reserved.
//

#import "JSONRPCService.h"
#import "JSONRPCMethodCall.h"
#import "JSONRPCResponseHandler.h"


/////////////////////////////////////////////////////////////////////////////
// MARK: Useful macros
//! @file JSONRPC.h
//! @brief Common header that include other headers of the framework. Contains useful macros too.
/////////////////////////////////////////////////////////////////////////////

//! Commodity macro to create an NSArray with arbitrary number of objects.
#define mkArray(...) [NSArray arrayWithObjects:__VA_ARGS__,nil]
//! Commodity macro to create a NSDictionary with arbitrary number of key/value pairs. Parameters are listed in the order objectN,keyN
#define mkDict(...) [NSDictionary dictionaryWithObjectsAndKeys:__VA_ARGS__,nil]
//! Commodity macro to create an NSNumber from an int
#define mkInt(x) [NSNumber numberWithInt:x]
//! Commodity macro to extract an int from an NSNumber
#define rdInt(nsx) [nsx intValue]






/////////////////////////////////////////////////////////////////////////////
// MARK: -
// MARK: Doxygen Documentation
/////////////////////////////////////////////////////////////////////////////


/**
 * @mainpage Ali's JSON-RPC Framework documentation
 *
 * @par Description
 *    This framework aims to provide JSON-RPC support to your Objective-C programs.
 *    Its role is to have a simple API to query any WebService that can be called using JSON-RPC.
 *    \n
 *    This framework currently support JSON-RPC 1.0 and 2.0.
 *
 * - @subpage Overview
 * - @subpage ErrMgmt
 *    - @subpage ErrCatch
 *    - @subpage ErrCodes
 * - @subpage Example
 *
 ***** <hr>
 *
 * @author O.Halligon
 * @version 1.0
 * @date May 2010
 *
 * @warning Even if the JSON object returned by the server is not normalized, this framework expect the object in the 'error'
 *          key of the JSON response to  have the same structure than the one defined in the JSON-RPC 2.0 specification,
 *          i.e. a JSON complex object containing the three keys/properties "code", "message" and "data", as
 *          NSError objects are constructing using the "code", "message" and "data" properties of the JSON error object.
 *
 * @note The JSON part is assured through the <a href="http://code.google.com/p/json-framework/">SBJSON</a> framework.
 *
 */


/**
 * @page Overview Overview
 * 
 * Briefely, you use this framework like this:<ol>
 * 
 * <li>Create a JSONRPCService object by passing the URL of the WebService:</li>
 * @code
 * JSONRPCService* service = [[JSONRPCService alloc] initWithURL:kServiceURL];
 * @endcode
 *
 * <li>Call a remote procedure of the WebService using JSON-RPC. To do this, you have <b>multiple equivalent possibilities</b>:
 * <ul>
 *
 *   <li> Use the JSONRPCService#callMethodWithName:parameters: method,
 *       passing it the name of the method to call (an NSString) and its parameters (an NSArray)</li>
 *
 *   <li> Use the JSONRPCService#callMethodWithNameAndParams: method,
 *       passing it the name (NSString) of the method to call, followed by a variable number of arguments
 *       representing the parameters, terminated by 'nil'.</li>
 *
 *   <li> Create a JSONRPCMethodCall object, passing it the name of the method to call and its arguments,
 *       then pass it to the JSONRPCService#callMethod: to actually call the method.
 *       (This is actually what the two previous options do in their implementation).</li>
 *
 *   <li> Ask the JSONRPCService for a proxy object (see JSONRPCService#proxy), then call directly any method you want
 *      as an Objective-C call (as if it was the WebService itself), with the arguments in an NSArray.
 *      The following 3 lines will in fact be equivalent:
 *      @code
 *        [service.proxy echo:[NSArray arrayWithObject:@"Hello there"]]
 *        [service callMethodWithName:@"echo" parameters:[NSArray arrayWithObject:@"Hello there"]];
 *        [service callMethodWithNameAndParams:@"echo",@"Hello there",nil];
 *      @endcode
 *   </li>
 * 
 * </ul></li>
 *
 * <li> To handle the response from the server, use the JSONRPCResponseHandler object returned by the method used to make the call:
 *   <ul>
 *     <li> You can set a delegate and callback to call when the response comes.
 *          The callback should be a \@selector that accept 3 parameters : a JSONRPCMethodCall object, a generic object representing the response, and an NSError.
 *     </li>
 *     <li> You can also set the Class you want the result to be converted into.
 *       <ul>
 *         <li> This class should respond to initWithJson: to be initialized using the JSON object</li>
 *         <li> Optionally, this class may also respond to -jsonProxy to be converted back to a JSON object if needed later (see SBJSON documentation) </li>
 *         <li> If the JSON object returned by the WebService is an array, the convertion from the JSON object to the given Class
 *          will be done on the objects in this array, so that e.g. an array of objects representing a person could be converted
 *          into an NSArray of Person objects.</li>
 *       </ul>
 *     </li>
 *   </ul>
 *   @code
 *   JSONRPCResponseHandler* h = [service callMethodWithNameAndParams:@"echo",@"Hello there",nil];
 *   [h setDelegate:self callback:@selector(methodCall:didReturnResult:error:)];
 *   [h setResultClass:[MyCustomClass class]]; // optional, don't write this line if you can't to receive the JSON object directly
 *   @endcode
 *   Or more concisely:
 *   @code
 *   [[service callMethodWithNameAndParams:@"echo",@"Hello there",nil]
 *    setDelegate:self callback:@selector(methodCall:didReturnResult:error) resultClass:[MyCustomClass class]];
 *   @endcode
 * </li>
 *
 * <li> Of course don't forget to release your JSONRPCService when you are done. </li>
 * </ol>
 *
 * For more information, you can look at the @ref Example.
 */


/**
 * @page ErrMgmt Error management
 *
 * @section ErrCatch Catching Errors
 *
 * @subsection ErrCatch1 Internal errors
 
 * When an internal error occur (network error, JSON parsing error, failed to convert to expected class, ...),
 * the JSONRPCResponseHandler will forward the error in the following order:
 *  - try to call @ref JSONRPCErrorHandler "methodCall:didFailWithError:" on the JSONRPCResponseHandler 's delegate
 *    (i.e. on the object that expect to receive the response)
 *  - if it does not respond, or respond and return YES, we try to call @ref JSONRPCErrorHandler "methodCall:didFailWithError:"
 *    on the JSONRPCService 's delegate
 *
 * This way, if you don't implement @ref JSONRPCErrorHandler "methodCall:didFailWithError:" in the delegate object that expect the response,
 *  it will fall back to the implementation of the JSONRPCService's delegate to handle generic cases (which typically display an alert or something).
 *
 * If you want to catch the error on specific cases, you still can implement @ref JSONRPCErrorHandler "methodCall:didFailWithError:" in the
 *  JSONRPCResponseHandler's delegate object to catch it. At this point, you can return YES to still execute the default behavior
 *  (the one in your JSONRPCService's delegate implementation) or return NO to avoid forwarding the error.
 *
 * @subsection ErrCatch2 Server errors
 *
 * For error returned by the server (a JSON is received but it contains an "error" object), indicating something
 * went wrong in the server (unknown method name, bad parameters, method-specific errors, ...), this object
 * is simply passed as an NSError in the third parameter of the JSONRPCResponseHandler's delegate callback.
 *
 ****************************************************************************
 *
 * @section ErrCodes The different error cases and their error domains
 * 
 * @subsection ErrCodes1 Internal errors ("methodCall:didFailWithError:")
 * This method can receive these kinds of errors:<ul>
 *
 * <li>Network error (Domain NSURLErrorDomain)</li>
 *
 * <li>JSON Parsing error (Domain SBJSONErrorDomain), whose userInfo dictionary contains the following keys:<ul>
 *   <li>key JSONRPCErrorDataKey : String we tried to parse</li>
 *   <li>key NSUnderlyingErrorKey : contains an underlying error if any</li>
 * </ul></li>
 * 
 * <li>JSON-to-Object Conversion error or internal errors (Domain JSONRPCInternalErrorDomain), whose userInfo dictionary contains the following keys:<ul>
 *   <li>key JSONRPCErrorDataKey : the JSON object we tried to convert</li>
 *   <li>key JSONRPCErrorClassNameKey : the name of the class we tried to convert to</li>
 * </ul></li>
 *
 * </ul>
 *
 * @subsection ErrCodes2 Server errors ("methodCall:didReturnResult:error:")
 * This section only receive error that comes directly from the WebService you query using JSON-RPC.
 * When the server returned an error in its JSON response, this is obviously a server-dependant error code.
 *
 * - The Domain for those errors is JSONRPCServerErrorDomain
 * - The userInfo dictionary contains server-specific error data in the JSONRPCErrorDataKey key
 *
 * @note This framework expect the WebService to return  error objects structured like this (inspired from JSON-RPC 2.0 specification):
 * @code
 * {
 *    code: // A Number that indicates the error type that occurred. See also reserved code at http://xmlrpc-epi.sourceforge.net/specs/rfc.fault_codes.php
 *    message: // A String providing a short description of the error. The message SHOULD be limited to a concise single sentence.
 *    data: // A value that contains additional information about the error. Defined by the Server (e.g. detailed error information, nested errors etc.).
 * }
 * @endcode
 */



/**
 * @page Example Example
 *
 *
 * @section Example_TestClass Test class: a demo of using the JSONRPC Framework
 *
 *     @code
 *     #import "JSONRPC.h"
 *     @interface TestClass : NSObject <JSONRPCErrorHandler>
 *     -(void)testIt;
 *     @end
 *     @endcode
 *
 *     @code
 *     #import "TestClass.h"
 *     #import "Person.h"
 *
 *     @implementation TestClass
 *     -(void)testIt
 *     {
 *       JSONRPCService* service = [[[JSONRPCService alloc] initWithURL:kServiceURL] autorelease];
 *
 *       [[service callMethodWithNameAndParams:@"getUserDetails",@"user1234",nil]
 *        setDelegate:self callback:@selector(methodCall:didReturnUser:error:) resultClass:[Person class]];
 *       // will convert the returned JSON into a Person object (providing that the class Person respond to -initWithJson, this is the case below)
 *     }
 *
 *     -(void)methodCall:(JSONRPCMethodCall*)meth didReturnUser:(Person*)p error:(NSError*)err
 *     {
 *       // We will effectively receive a "Person*" object (and not a JSON object like a NSDictionary) in the parameter p
 *       // because we asked to convert the result using [Person class] in -testIt
 *
 *       NSLog(@"Received person: %@",p);
 *       if (error) NSLog(@"error in method call: %@",err);
 *     }
 *
 *     -(BOOL)methodCall:(JSONRPCMethodCall*)meth didFailWithError:(NSError*)err
 *     {
 *       // handle the NSError (network error / no connection, etc.)
 *       return NO; // don't call methodCall:didFailWithError: on the JSONRPCService's delegate.
 *     }
 *     @end
 *     @endcode
 *
 *
 * @section Example_Person Person class: to represent in ObjC the persons returned by the JSONRPC WebService
 * Used in TestClass as the result is converted into a Person object ("... resultClass:[Person class]")
 *
 *     @code
 *     @interface Person : NSObject {
 *       NSString* firstName;
 *       NSString* lastName;
 *     }
 *     -(void)initWithJson:(id)jsonObj;
 *     @property(nonatomic,retain) NSString* firstName;
 *     @property(nonatomic,retain) NSString* lastName;
 *     @end
 *     @endcode
 *
 *     @code
 *     #import "Person.h"
 *
 *     @implementation Person
 *     @synthesize firstName,lastName;
 *     -(id)initWithJson:(id)jsonObj
 *     {
 *        if (self = [super init]) {
 *          self.firstName = [jsonObj objectForKey:@"firstname"];
 *          self.lastName = [jsonObj objectForKey:@"lastname"];
 *        }
 *        return self;
 *     }
 *     -(NSString*)description { return [NSString stringWithFormat:@"<Person %@ %@>",firstName,lastName]; }
 *     -(void)dealloc
 *     {
 *       [firstName release];
 *       [lastName release];
 *       [super dealloc];
 *     }
 *     @end
 *     @endcode
 *
 */



