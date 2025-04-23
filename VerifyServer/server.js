
const grpc = require('@grpc/grpc-js')
const message_proto = require('./proto')
const const_module = require('./const')
const { v4: uuidv4 } = require('uuid');
const emailModule = require('./email');
const redis_module = require('./redis')

/**
 * GetVerifyCode grpc响应获取验证码的服务
 * @param {*} call 为grpc请求 
 * @param {*} callback 为grpc回调
 * @returns 
 */
async function GetVerifyCode(call, callback) {
    console.log("email is ", call.request.email)
    try{
        let query_res = await redis_module.GetRedis(const_module.code_prefix + call.request.email);
        console.log("query_res is ", query_res)
        if(query_res == null){

        }
        let uniqueId = query_res;
        if(query_res == null){
            uniqueId = uuidv4();
            if (uniqueId.length > 4) {
                uniqueId = uniqueId.substring(0, 4);
            } 
            let bres = await redis_module.SetRedisExpire(const_module.code_prefix+call.request.email, uniqueId,600)
            if(!bres){
                callback(null, { email:  call.request.email,
                    error:const_module.Errors.RedisErr
                });
                return;
            }
        }

        console.log("uniqueId is ", uniqueId)
        let text_str =  '您的验证码为'+ uniqueId +'请三分钟内完成注册'
        //发送邮件
        let mailOptions = {
            from: '1163153301@qq.com',
            //from: emailModule.config_module.email_user,
            to: call.request.email,
            subject: '验证码',
            text: text_str,
        };
    
        let send_res = await emailModule.SendMail(mailOptions);
        console.log("send res is ", send_res)

        callback(null, { email:  call.request.email,
            error:const_module.Errors.Success
        }); 
        
 
    }catch(error){
        console.log("catch error is ", error)

        callback(null, { email:  call.request.email,
            error:const_module.Errors.Exception
        }); 
    }
     
}

// function main() {
//     var server = new grpc.Server()
//     server.addService(message_proto.VerifyService.service, { GetVerifyCode: GetVerifyCode })
//     server.bindAsync('0.0.0.0:50051', grpc.ServerCredentials.createInsecure(), () => {
//         server.start()
//         console.log('varify server started')        
//     })
// }

// main()

async function main() {
    const server = new grpc.Server();
    server.addService(message_proto.VerifyService.service, { GetVerifyCode: GetVerifyCode });

    // 处理进程信号
    const exitHandler = async (signal) => {
        console.log(`\n收到 ${signal} 信号，开始优雅退出...`);
        
        // 尝试优雅关闭服务器
        server.tryShutdown(() => {
            console.log('gRPC 服务器已成功关闭');
            process.exit(0);
        });

        // 设置强制退出超时
        setTimeout(() => {
            console.log('强制关闭服务器（超时）');
            process.exit(1);
        }, 5000); // 5秒后强制退出
    };

    // 注册信号处理程序
    process.on('SIGTERM', () => exitHandler('SIGTERM'));
    process.on('SIGINT', () => exitHandler('SIGINT'));
    process.on('SIGHUP', () => exitHandler('SIGHUP'));

    // 处理未捕获的异常
    process.on('uncaughtException', (error) => {
        console.error('未捕获的异常:', error);
        exitHandler('uncaughtException');
    });

    // 处理未处理的 Promise 拒绝
    process.on('unhandledRejection', (reason, promise) => {
        console.error('未处理的 Promise 拒绝:', reason);
        exitHandler('unhandledRejection');
    });

    try {
        // 启动服务器
        await new Promise((resolve, reject) => {
            server.bindAsync('0.0.0.0:50051', grpc.ServerCredentials.createInsecure(), (err, port) => {
                if (err) {
                    console.error('服务器绑定失败:', err);
                    reject(err);
                    return;
                }
                console.log(`gRPC 服务器正在监听端口 ${port}`);
                resolve();
            });
        });
    } catch (error) {
        console.error('启动服务器时出错:', error);
        process.exit(1);
    }
}

// 启动服务器
main().catch(error => {
    console.error('主程序异常:', error);
    process.exit(1);
});
