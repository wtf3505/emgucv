#include "quaternions.h"

void eulerToQuaternions(double x, double y, double z, Quaternions* quaternions)
{
   double 
      halfX = x *0.5,
      halfY = y *0.5,
      halfZ = z *0.5,
      sinX = sin(halfX), 
      cosX = cos(halfX),
      sinY = sin(halfY),
      cosY = cos(halfY),
      sinZ = sin(halfZ),
      cosZ = cos(halfZ);

#if EMGU_SSE2
   __m128d cosSinY = _mm_set_pd(cosY, sinY);
   __m128d cosSinZ = _mm_set_pd(cosZ, sinZ);
   __m128d sinCosZ = _mm_set_pd(sinZ, cosZ);
   __m128d cosSinX = _mm_set_pd(cosX, sinX);  
   __m128d sinCosX = _mm_set_pd(sinX, cosX);
   __m128d tmp1 = _mm_mul_pd(cosSinY, cosSinZ);
   __m128d tmp2 = _mm_mul_pd(cosSinY, sinCosZ);

   quaternions->w = _dot_product(cosSinX, tmp1);
   quaternions->x = _cross_product(tmp1, cosSinX); 
   quaternions->y = _dot_product(sinCosX, tmp2);
   quaternions->z = _cross_product(tmp2, sinCosX); 

#else
   double cosYcosZ = cosY*cosZ;
   double sinYsinZ = sinY*sinZ;
   
   quaternions->w = cosX*cosYcosZ + sinX*sinYsinZ;
   quaternions->x = sinX*cosYcosZ - cosX*sinYsinZ;

   double cosYsinZ = cosY*sinZ;
   double sinYcosZ = sinY*cosZ;

   quaternions->y = cosX*sinYcosZ + sinX*cosYsinZ;
   quaternions->z = cosX*cosYsinZ - sinX*sinYcosZ;
#endif
}

void quaternionsToEuler(Quaternions* quaternions, double* x, double* y, double* z)
{
   double q0 = quaternions->w;
   double q1 = quaternions->x;
   double q2 = quaternions->y;
   double q3 = quaternions->z;

   *x = atan2(2.0 * (q0 * q1 + q2 * q3), 1.0 - 2.0 * (q1*q1 + q2*q2));
   *y = asin(2.0 * (q0 * q2 - q3 * q1));
   *z = atan2(2.0 * (q0 * q3 + q1 * q2), 1.0 - 2.0 * (q2*q2 + q3*q3));
}

void quaternionsToRotationMatrix(Quaternions* quaternions, CvMat* rotation)
{
   double w = quaternions->w;
   double x = quaternions->x;
   double y = quaternions->y;
   double z = quaternions->z;

   cv::Mat r = cv::cvarrToMat(rotation);
   CV_Assert(r.rows == 3 && r.cols == 3);
   cv::MatIterator_<double> rIter = r.begin<double>();
   *rIter++ = w*w+x*x-y*y-z*z; *rIter++ = 2.0*(x*y-w*z); *rIter++ = 2.0*(x*z+w*y);
   *rIter++ = 2.0*(x*y+w*z); *rIter++ = w*w-x*x+y*y-z*z; *rIter++ = 2.0*(y*z-w*x);
   *rIter++ = 2.0*(x*z-w*y); *rIter++ = 2.0*(y*z+w*x); *rIter++ = w*w-x*x-y*y+z*z;
}

inline void quaternionsRotate(double w, double x, double y, double z, double v1, double v2, double v3, double*vr)
{
   double
      t2 =   w*x,
      t3 =   w*y,
      t4 =   w*z,
      t5 =  -x*x,
      t6 =   x*y,
      t7 =   x*z,
      t8 =  -y*y,
      t9 =   y*z,
      t10 = -z*z;

   *vr++  = 2.0*( (t8 + t10)* v1 + (t6 -  t4)* v2 + (t3 + t7)* v3 ) + v1,
   *vr++ = 2.0*( (t4 +  t6)* v1 + (t5 + t10)* v2 + (t9 - t2)* v3 ) + v2,
   *vr = 2.0*( (t7 -  t3)* v1 + (t2 +  t9)* v2 + (t5 + t8)* v3 ) + v3;
}

void quaternionsRotatePoint(Quaternions* quaternions, CvPoint3D64f* point, CvPoint3D64f* pointDst)
{
   quaternionsRotate(quaternions->w, quaternions->x, quaternions->y, quaternions->z, point->x, point->y, point->z, (double*) pointDst);
}

void quaternionsRotatePoints(Quaternions* quaternions,  CvMat* pointSrc, CvMat* pointDst)
{
   cv::Mat p = cv::cvarrToMat(pointSrc);
   cv::Mat pDst = cv::cvarrToMat(pointDst);
   CV_Assert((p.rows == 3 && p.cols == 1) || p.cols ==3);
   CV_Assert(pDst.rows == p.rows && pDst.cols == p.cols);

   double *v;

   cv::MatIterator_<double> pIter = p.begin<double>();
   cv::MatIterator_<double> pDstIter = pDst.begin<double>();

   if ((p.rows == 3 && p.cols == 1))
   {  
      v = pIter.ptr;
      quaternionsRotate(quaternions->w, quaternions->x, quaternions->y, quaternions->z, *v, *(v+1), *(v+2), pDstIter.ptr);
   } else 
   {
      for(int i = 0; i < p.rows; i++, pIter+=3, pDstIter+=3)
      {
         v = pIter.ptr;
         quaternionsRotate(quaternions->w, quaternions->x, quaternions->y, quaternions->z, *v, *(v+1), *(v+2),  pDstIter.ptr);
      }
   }
}

void quaternionsMultiply(Quaternions* quaternions1, Quaternions* quaternions2, Quaternions* quaternionsDst)
{
#if EMGU_SSE2
   __m128d _w1w1 = _mm_set1_pd(quaternions1->w);
   __m128d _x1x1 = _mm_set1_pd(quaternions1->x);
   __m128d _y1y1 = _mm_set1_pd(quaternions1->y);
   __m128d _z1z1 = _mm_set1_pd(quaternions1->z);
   __m128d _w2x2 = _mm_set_pd(quaternions2->w, quaternions2->x);
   __m128d _nx2w2 = _mm_set_pd(-quaternions2->x, quaternions2->w);
   __m128d _z2y2 = _mm_set_pd(quaternions2->z, quaternions2->y);
   __m128d _ny2z2 = _mm_set_pd(-quaternions2->y, quaternions2->z);

   __m128d _wx = _mm_add_pd(
      _mm_add_pd(_mm_mul_pd(_w1w1, _w2x2), _mm_mul_pd(_x1x1, _nx2w2)),
      _mm_sub_pd(_mm_mul_pd(_y1y1, _ny2z2), _mm_mul_pd(_z1z1, _z2y2)));
   __m128d _zy = _mm_add_pd(
      _mm_sub_pd(_mm_mul_pd(_w1w1, _z2y2), _mm_mul_pd(_x1x1, _ny2z2)),
      _mm_add_pd(_mm_mul_pd(_y1y1, _nx2w2), _mm_mul_pd(_z1z1, _w2x2)));
   
   quaternionsDst->w = _wx.m128d_f64[1];
   quaternionsDst->x = _wx.m128d_f64[0];
   quaternionsDst->z = _zy.m128d_f64[1];
   quaternionsDst->y = _zy.m128d_f64[0];
#else
   double w1 = quaternions1->w;
   double x1 = quaternions1->x;
   double y1 = quaternions1->y;
   double z1 = quaternions1->z;

   double w2 = quaternions2->w;
   double x2 = quaternions2->x;
   double y2 = quaternions2->y;
   double z2 = quaternions2->z;

   quaternionsDst->w = (w1*w2 - x1*x2 - y1*y2 - z1*z2);
   quaternionsDst->x = (w1*x2 + x1*w2 + y1*z2 - z1*y2);

   quaternionsDst->z = (w1*z2 + x1*y2 - y1*x2 + z1*w2);
   quaternionsDst->y = (w1*y2 - x1*z2 + y1*w2 + z1*x2);

#endif
}

/* convert axis angle vector to quaternions. (x,y,z) is the rotatation axis and |(x,y,z)| is the rotation angle  */
void axisAngleToQuaternions(CvPoint3D64f* axisAngle, Quaternions* quaternions)
{
   double theta = sqrt(axisAngle->x * axisAngle->x + axisAngle->y * axisAngle->y + axisAngle->z * axisAngle->z);
   if (theta < 1.0e-16) 
   {
      quaternions->w = 1.0e-16;
      quaternions->x = 1.0;
      quaternions->y = 0.0;
      quaternions->x = 0.0;
      quaternionsRenorm(quaternions);
      return;
   }
   double halfAngle = theta * 0.5;
   double sinHalfAngle = sin(halfAngle);
   double scale = sinHalfAngle / theta;
   quaternions->w = cos(halfAngle);
   quaternions->x = axisAngle->x * scale;
   quaternions->y = axisAngle->y * scale;
   quaternions->z = axisAngle->z * scale;

   quaternionsRenorm(quaternions);
}

/* convert quaternions to axis angle vector. (x,y,z) is the rotatation matrix and |(x,y,z)| is the rotation angle  */
void quaternionsToAxisAngle(Quaternions* quaternions, CvPoint3D64f* axisAngle)
{
   double theta = 2.0 * acos(quaternions->w);
   if (theta == 0)
   {
      axisAngle->x = axisAngle->y = axisAngle->z = 1.0e-16;
   } else
   {
      double norm = sqrt(quaternions->x * quaternions->x + quaternions->y * quaternions->y + quaternions->z * quaternions->z);
      double scale = theta / norm;
      axisAngle->x = quaternions->x * scale;
      axisAngle->y = quaternions->y * scale;
      axisAngle->z = quaternions->z * scale;
   }
}

void quaternionsRenorm(Quaternions* quaternions)
{
   double norm = sqrt(quaternions->w * quaternions->w 
      + quaternions->x * quaternions->x 
      + quaternions->y * quaternions->y 
      + quaternions->z * quaternions->z);
   
   double scale = 1.0 / norm;

   quaternions->w *= scale;
   quaternions->x *= scale;
   quaternions->y *= scale;
   quaternions->z *= scale;
}

void quaternionsSlerp(Quaternions* qa, Quaternions* qb, double t, Quaternions* qm)
{
   // Calculate angle between them.
   double cosHalfTheta = qa->w * qb->w + qa->x * qb->x + qa->y * qb->y + qa->z * qb->z;
   // if qa=qb or qa=-qb then theta = 0 and we can return qa
   if (abs(cosHalfTheta) >= 1.0){
      qm->w = qa->w;qm->x = qa->x;qm->y = qa->y;qm->z = qa->z;
      return;
   }
   // Calculate temporary values.
   double halfTheta = acos(cosHalfTheta);
   double sinHalfTheta = sqrt(1.0 - cosHalfTheta*cosHalfTheta);
   // if theta = 180 degrees then result is not fully defined
   // we could rotate around any axis normal to qa or qb
   if (fabs(sinHalfTheta) < 0.001){ // fabs is floating point absolute
      qm->w = (qa->w * 0.5 + qb->w * 0.5);
      qm->x = (qa->x * 0.5 + qb->x * 0.5);
      qm->y = (qa->y * 0.5 + qb->y * 0.5);
      qm->z = (qa->z * 0.5 + qb->z * 0.5);
      return;
   }
   double ratioA = sin((1 - t) * halfTheta) / sinHalfTheta;
   double ratioB = sin(t * halfTheta) / sinHalfTheta; 
   //calculate Quaternion.
   qm->w = (qa->w * ratioA + qb->w * ratioB);
   qm->x = (qa->x * ratioA + qb->x * ratioB);
   qm->y = (qa->y * ratioA + qb->y * ratioB);
   qm->z = (qa->z * ratioA + qb->z * ratioB);
}