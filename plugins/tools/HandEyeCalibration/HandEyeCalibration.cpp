/**
 * @file HandEyeCalibration.cpp
 * @brief 手眼标定工具 — 求解相机与机器人末端(手)的坐标变换 (对标 CKVision 手眼标定)
 *
 * 原理: 经典 AX = XB 手眼方程。
 *   A_i = 机器人基座→末端 位姿 (gripper2base, 由机器人数采得)
 *   B_i = 标定板→相机 位姿 (target2cam, 由棋盘格外参求得)
 *   X   = 相机→末端 手眼矩阵 (待求, 常数)
 * 用 cv::calibrateHandEye 求解 X, 并输出残差评估标定质量。
 *
 * 输入(属性, 以 "x,y,z;x,y,z;..." 罗德里格斯向量+平移列表):
 *   rBase/tBase  机器人基座→末端 位姿组 (≥3)
 *   rCam/tCam    标定板→相机 位姿组
 *   method       标定算法: Tsai / Park / Horaud / Andreff
 *   queryX/Y/Z   相机坐标系下的查询点 → 输出末端/基座坐标系坐标
 *
 * 输出:
 *   handeyeRvec/Tvec  相机→末端 旋转向量/平移
 *   handeyeRms        平均残差(AX=XB 一致性, 角度°/平移mm)
 *   resultX/Y/Z       查询点变换到机器人基座坐标系
 * 上下文: handeye_* (rvec/tvec/rms)
 */
#include "HandEyeCalibration.h"
#include "../../../src/engine/ToolRegistry.h"
#include "../../../src/core/GlobalVariables.h"
#ifdef VI_HAS_OPENCV
#include <opencv2/calib.hpp>
#include <opencv2/geometry/3d.hpp>
#endif
#include <cmath>
#include <algorithm>
#include <QStringList>

namespace VisionInspector {

PropertyDefList HandEyeCalibration::propertyDefs() const {
    return {
        PropertyDef::stringProp("rBase", "基座→末端 旋转组(rvec)", ""),
        PropertyDef::stringProp("tBase", "基座→末端 平移组", ""),
        PropertyDef::stringProp("rCam", "标定板→相机 旋转组(rvec)", ""),
        PropertyDef::stringProp("tCam", "标定板→相机 平移组", ""),
        PropertyDef::enumProp("method", "标定算法",
                              {"Tsai", "Park", "Horaud", "Andreff"}, 0),
        PropertyDef::doubleProp("queryX", "查询点X(相机系)", 0, -1e9, 1e9),
        PropertyDef::doubleProp("queryY", "查询点Y(相机系)", 0, -1e9, 1e9),
        PropertyDef::doubleProp("queryZ", "查询点Z(相机系)", 0, -1e9, 1e9),
        PropertyDef::enumProp("applyTo", "应用范围", {"当前流程", "所有流程"}, 0),
    };
}

namespace {
// 解析 "x,y,z;x,y,z;..." 罗德里格斯/平移组
bool parsePoses(const QString& text, std::vector<cv::Vec3d>& poses) {
    poses.clear();
    const QStringList groups = text.split(';', Qt::SkipEmptyParts);
    for (const QString& g : groups) {
        const QStringList v = g.trimmed().split(',');
        if (v.size() != 3) return false;
        bool ok[3] = {false, false, false};
        const double x = v[0].trimmed().toDouble(&ok[0]);
        const double y = v[1].trimmed().toDouble(&ok[1]);
        const double z = v[2].trimmed().toDouble(&ok[2]);
        if (!ok[0] || !ok[1] || !ok[2]) return false;
        poses.push_back({x, y, z});
    }
    return true;
}

// 由 rvec/tvec 构造 4x4 齐次矩阵
cv::Mat poseToMat(const cv::Vec3d& rvec, const cv::Vec3d& tvec) {
    cv::Mat R;
    cv::Rodrigues(cv::Mat(rvec), R);
    cv::Mat M = cv::Mat::eye(4, 4, CV_64F);
    R.copyTo(M(cv::Rect(0, 0, 3, 3)));
    M.at<double>(0, 3) = tvec[0];
    M.at<double>(1, 3) = tvec[1];
    M.at<double>(2, 3) = tvec[2];
    return M;
}

// 两矩阵旋转部分相对角差(deg) 与平移距离
void poseResidual(const cv::Mat& A, const cv::Mat& B, double& rotDeg, double& transDist) {
    rotDeg = 0; transDist = 0;
    const cv::Mat Rdiff = A(cv::Rect(0, 0, 3, 3)) * B(cv::Rect(0, 0, 3, 3)).t();
    double tr = cv::trace(Rdiff)[0];
    tr = std::max(-1.0, std::min(3.0, tr));
    rotDeg = std::acos((tr - 1.0) / 2.0) * 180.0 / M_PI;
    transDist = cv::norm(A(cv::Rect(3, 0, 1, 3)) - B(cv::Rect(3, 0, 1, 3)));
}

// 旋转矩阵 → 四元数 (w,x,y,z)
cv::Vec4d rotToQuat(const cv::Mat& R) {
    double w = std::sqrt(std::max(0.0, 1.0 + R.at<double>(0,0) + R.at<double>(1,1) + R.at<double>(2,2))) / 2.0;
    const double w4 = 4.0 * w;
    if (w4 > 1e-8) {
        return {w,
                (R.at<double>(2,1) - R.at<double>(1,2)) / w4,
                (R.at<double>(0,2) - R.at<double>(2,0)) / w4,
                (R.at<double>(1,0) - R.at<double>(0,1)) / w4};
    }
    // w≈0 时退化情况 (180°旋转)
    const double x = std::sqrt(std::max(0.0, 1.0 + R.at<double>(0,0) - R.at<double>(1,1) - R.at<double>(2,2))) / 2.0;
    const double y = std::sqrt(std::max(0.0, 1.0 - R.at<double>(0,0) + R.at<double>(1,1) - R.at<double>(2,2))) / 2.0;
    const double z = std::sqrt(std::max(0.0, 1.0 - R.at<double>(0,0) - R.at<double>(1,1) + R.at<double>(2,2))) / 2.0;
    return {0, x, y, z};
}
} // namespace

bool HandEyeCalibration::execute(ToolContext& context) {
#ifdef VI_HAS_OPENCV
    const QString rBaseT = propertyValue("rBase").toString();
    const QString tBaseT = propertyValue("tBase").toString();
    const QString rCamT  = propertyValue("rCam").toString();
    const QString tCamT  = propertyValue("tCam").toString();
    const int method     = propertyValue("method").toInt();

    std::vector<cv::Vec3d> rBase, tBase, rCam, tCam;
    if (!parsePoses(rBaseT, rBase) || !parsePoses(tBaseT, tBase) ||
        !parsePoses(rCamT, rCam) || !parsePoses(tCamT, tCam)) {
        setResultData("error", "位姿格式错误: 应 rBase/tBase/rCam/tCam 各为 x,y,z;x,y,z;...");
        setStatus(ToolStatus::NG);
        return false;
    }
    const size_t n = std::min({rBase.size(), tBase.size(), rCam.size(), tCam.size()});
    if (n < 3) {
        setResultData("error", QString("有效位姿组%1<3, 手眼标定需≥3组").arg(n));
        setStatus(ToolStatus::NG);
        return false;
    }

    // 组装 calibrateHandEye 输入 (旋转用 3x3 矩阵)
    std::vector<cv::Mat> R_base, t_base, R_cam, t_cam;
    for (size_t i = 0; i < n; ++i) {
        cv::Mat R;
        cv::Rodrigues(cv::Mat(rBase[i]), R);
        R_base.push_back(R.clone());
        t_base.push_back((cv::Mat_<double>(3, 1) << tBase[i][0], tBase[i][1], tBase[i][2]));
        cv::Mat Rc;
        cv::Rodrigues(cv::Mat(rCam[i]), Rc);
        R_cam.push_back(Rc.clone());
        t_cam.push_back((cv::Mat_<double>(3, 1) << tCam[i][0], tCam[i][1], tCam[i][2]));
    }

    cv::Mat R_cam2grip, t_cam2grip;
    int hmethod = cv::CALIB_HAND_EYE_TSAI;
    if (method == 1) hmethod = cv::CALIB_HAND_EYE_PARK;
    else if (method == 2) hmethod = cv::CALIB_HAND_EYE_HORAUD;
    else if (method == 3) hmethod = cv::CALIB_HAND_EYE_ANDREFF;
    try {
        cv::calibrateHandEye(R_base, t_base, R_cam, t_cam,
                             R_cam2grip, t_cam2grip,
                             static_cast<cv::HandEyeCalibrationMethod>(hmethod));
    } catch (const cv::Exception& e) {
        setResultData("error", QString("手眼标定失败: %1").arg(e.what()));
        setStatus(ToolStatus::NG);
        return false;
    }
    if (R_cam2grip.empty() || t_cam2grip.empty()) {
        setResultData("error", "手眼标定返回空结果");
        setStatus(ToolStatus::NG);
        return false;
    }
    // 手眼矩阵 X (4x4)
    cv::Mat Xm = cv::Mat::eye(4, 4, CV_64F);
    R_cam2grip.copyTo(Xm(cv::Rect(0, 0, 3, 3)));
    Xm.at<double>(0, 3) = t_cam2grip.at<double>(0);
    Xm.at<double>(1, 3) = t_cam2grip.at<double>(1);
    Xm.at<double>(2, 3) = t_cam2grip.at<double>(2);

    // 残差评估: 闭环一致性 A·X·B = C (常数, 基座→标定板)
    // 对每组求 C_i = A_i·X·B_i; 各组间应一致, 用四元数中值旋转+中值平移做基准,
    // 各组相对偏差即标定误差 (不受 C 绝对位姿影响)
    std::vector<cv::Mat> chainList;
    std::vector<cv::Vec4d> quats;
    std::vector<double> txArr, tyArr, tzArr;
    chainList.reserve(n); quats.reserve(n);
    txArr.reserve(n); tyArr.reserve(n); tzArr.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        const cv::Mat A = poseToMat(rBase[i], tBase[i]);
        const cv::Mat B = poseToMat(rCam[i], tCam[i]);
        const cv::Mat Ci = A * Xm * B;
        chainList.push_back(Ci);
        quats.push_back(rotToQuat(Ci(cv::Rect(0, 0, 3, 3))));
        txArr.push_back(Ci.at<double>(0, 3));
        tyArr.push_back(Ci.at<double>(1, 3));
        tzArr.push_back(Ci.at<double>(2, 3));
    }
    // 四元数平均 (保证 q 与 q0 同号, 即半球一致)
    cv::Vec4d qAvg = quats[0];
    for (size_t i = 1; i < n; ++i) {
        cv::Vec4d q = quats[i];
        if (q.dot(qAvg) < 0) q = -q;
        qAvg = qAvg + q;
    }
    const double qn = std::sqrt(qAvg.dot(qAvg));
    if (qn > 1e-12) qAvg = qAvg / qn; else qAvg = {1, 0, 0, 0};
    // 四元数 → 旋转矩阵 (中值旋转)
    const double qw = qAvg[0], qx = qAvg[1], qy = qAvg[2], qz = qAvg[3];
    cv::Mat Rmed = (cv::Mat_<double>(3, 3) <<
        1 - 2 * (qy * qy + qz * qz), 2 * (qx * qy - qz * qw),   2 * (qx * qz + qy * qw),
        2 * (qx * qy + qz * qw),     1 - 2 * (qx * qx + qz * qz), 2 * (qy * qz - qx * qw),
        2 * (qx * qz - qy * qw),     2 * (qy * qz + qx * qw),    1 - 2 * (qx * qx + qy * qy));
    auto med = [](std::vector<double> v) {
        std::sort(v.begin(), v.end());
        return v[v.size() / 2];
    };
    const double medTx = med(txArr), medTy = med(tyArr), medTz = med(tzArr);
    double sumRot = 0, sumTrans = 0, maxRot = 0, maxTrans = 0;
    for (size_t i = 0; i < n; ++i) {
        cv::Mat Cref = cv::Mat::eye(4, 4, CV_64F);
        Rmed.copyTo(Cref(cv::Rect(0, 0, 3, 3)));
        Cref.at<double>(0, 3) = medTx; Cref.at<double>(1, 3) = medTy; Cref.at<double>(2, 3) = medTz;
        double rd, td;
        poseResidual(chainList[i], Cref, rd, td);
        sumRot += rd; sumTrans += td;
        maxRot = std::max(maxRot, rd);
        maxTrans = std::max(maxTrans, td);
    }
    const double rmsRot = sumRot / n, rmsTrans = sumTrans / n;

    // 输出结果
    cv::Mat rvecOut;
    cv::Rodrigues(R_cam2grip, rvecOut);
    const double rx = rvecOut.at<double>(0), ry = rvecOut.at<double>(1), rz = rvecOut.at<double>(2);
    const double tx = t_cam2grip.at<double>(0), ty = t_cam2grip.at<double>(1), tz = t_cam2grip.at<double>(2);
    setResultData("handeyeRvec", QVariantList{rx, ry, rz});
    setResultData("handeyeTvec", QVariantList{tx, ty, tz});
    setResultData("handeyeRms", rmsRot);
    setResultData("handeyeRmsTrans", rmsTrans);
    setResultData("maxRotErr", maxRot);
    setResultData("maxTransErr", maxTrans);
    setResultData("usedPoses", (int)n);
    setResultData("calibrated", true);

    // 查询点: 相机系 → 基座系 (若 applyTo 需先经末端)
    const double pX = propertyValue("queryX").toDouble();
    const double pY = propertyValue("queryY").toDouble();
    const double pZ = propertyValue("queryZ").toDouble();
    // 相机系点 → 末端系 (X 变换)
    cv::Mat p = (cv::Mat_<double>(4, 1) << pX, pY, pZ, 1.0);
    const cv::Mat pEnd = Xm * p;
    const double ex = pEnd.at<double>(0) / pEnd.at<double>(3);
    const double ey = pEnd.at<double>(1) / pEnd.at<double>(3);
    const double ez = pEnd.at<double>(2) / pEnd.at<double>(3);
    // 末端系 → 基座系 (用最后一组基座→末端位姿)
    double bx = ex, by = ey, bz = ez;
    if (n > 0) {
        const cv::Mat Tbase = poseToMat(rBase[n - 1], tBase[n - 1]);
        const cv::Mat pb = Tbase * pEnd;
        bx = pb.at<double>(0) / pb.at<double>(3);
        by = pb.at<double>(1) / pb.at<double>(3);
        bz = pb.at<double>(2) / pb.at<double>(3);
    }
    setResultData("resultX", bx);
    setResultData("resultY", by);
    setResultData("resultZ", bz);

    // 上下文
    context.setData("handeye_rx", rx); context.setData("handeye_ry", ry); context.setData("handeye_rz", rz);
    context.setData("handeye_tx", tx); context.setData("handeye_ty", ty); context.setData("handeye_tz", tz);
    context.setData("handeye_rms", rmsRot);

    if (propertyValue("applyTo").toInt() == 1) {
        if (auto* gv = context.globalVariables()) {
            gv->set("handeye_rx", rx); gv->set("handeye_ry", ry); gv->set("handeye_rz", rz);
            gv->set("handeye_tx", tx); gv->set("handeye_ty", ty); gv->set("handeye_tz", tz);
            gv->set("handeye_rms", rmsRot);
        }
    }

    setStatus(ToolStatus::OK);
    return true;
#else
    setResultData("error", "需要OpenCV库");
    setStatus(ToolStatus::NG);
    return false;
#endif
}

} // namespace VisionInspector

VI_REGISTER_TOOL(HandEyeCalibration, "手眼标定", VisionInspector::ToolCategory::Calibration)
