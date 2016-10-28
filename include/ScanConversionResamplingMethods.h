/*=========================================================================
 *
 *  Copyright Insight Software Consortium
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/

#ifndef ScanConversionResamplingMethods_h
#define ScanConversionResamplingMethods_h

#include "itkResampleImageFilter.h"
#include "itkNearestNeighborInterpolateImageFunction.h"
#include "itkLinearInterpolateImageFunction.h"
#include "itkWindowedSincInterpolateImageFunction.h"
#include "itkSpecialCoordinatesImageToVTKStructuredGridFilter.h"
#include "itkVTKImageToImageFilter.h"
#include "itkImageAlgorithm.h"

#include "vtkProbeFilter.h"
#include "vtkImageData.h"
#include "vtkNew.h"
#include "vtkStructuredGrid.h"
#include "vtkPointInterpolator.h"
#include "vtkGaussianKernel.h"
#include "vtkLinearKernel.h"
#include "vtkShepardKernel.h"
#include "vtkInterpolationKernel.h"
#include "vtkVoronoiKernel.h"

#include "itkPluginFilterWatcher.h"

namespace
{

enum ScanConversionResamplingMethod {
  ITK_NEAREST_NEIGHBOR = 0,
  ITK_LINEAR,
  ITK_WINDOWED_SINC,
  VTK_PROBE_FILTER,
  VTK_GAUSSIAN_KERNEL,
  VTK_LINEAR_KERNEL,
  VTK_SHEPARD_KERNEL,
  VTK_VORONOI_KERNEL
};


template< typename TInputImage, typename TOutputImage >
int
ITKScanConversionResampling(const typename TInputImage::Pointer & inputImage,
  typename TOutputImage::Pointer & outputImage,
  const typename TOutputImage::SizeType & size,
  const typename TOutputImage::SpacingType & spacing,
  const typename TOutputImage::PointType & origin,
  const typename TOutputImage::DirectionType & direction,
  ScanConversionResamplingMethod method,
  ModuleProcessInformation * CLPProcessInformation
  )
{
  typedef TInputImage  InputImageType;
  typedef TOutputImage OutputImageType;
  typedef double       CoordRepType;

  typedef itk::ResampleImageFilter< InputImageType, OutputImageType > ResamplerType;
  typename ResamplerType::Pointer resampler = ResamplerType::New();
  resampler->SetInput( inputImage );

  resampler->SetSize( size );
  resampler->SetOutputSpacing( spacing );
  resampler->SetOutputOrigin( origin );
  resampler->SetOutputDirection( direction );
  switch( method )
    {
  case ITK_NEAREST_NEIGHBOR:
      {
      typedef itk::NearestNeighborInterpolateImageFunction< InputImageType, CoordRepType > InterpolatorType;
      typename InterpolatorType::Pointer interpolator = InterpolatorType::New();
      resampler->SetInterpolator( interpolator );
      break;
      }
  case ITK_LINEAR:
      {
      typedef itk::LinearInterpolateImageFunction< InputImageType, CoordRepType > InterpolatorType;
      typename InterpolatorType::Pointer interpolator = InterpolatorType::New();
      resampler->SetInterpolator( interpolator );
      break;
      }
  case ITK_WINDOWED_SINC:
      {
      static const unsigned int Radius = 3;
      typedef itk::Function::LanczosWindowFunction< Radius, CoordRepType, CoordRepType > WindowFunctionType;
      typedef itk::WindowedSincInterpolateImageFunction< InputImageType, Radius, WindowFunctionType > InterpolatorType;
      typename InterpolatorType::Pointer interpolator = InterpolatorType::New();
      resampler->SetInterpolator( interpolator );
      break;
      }
  default:
    std::cerr << "Unsupported resampling method in ITKScanConversionResampling" << std::endl;
    return EXIT_FAILURE;
    }

  itk::PluginFilterWatcher watchResampler(resampler, "Resample Image", CLPProcessInformation);
  resampler->Update();
  outputImage = resampler->GetOutput();

  return EXIT_SUCCESS;
}


template< typename TInputImage, typename TOutputImage >
int
VTKProbeFilterResampling(const typename TInputImage::Pointer & inputImage,
  typename TOutputImage::Pointer & outputImage,
  const typename TOutputImage::SizeType & size,
  const typename TOutputImage::SpacingType & spacing,
  const typename TOutputImage::PointType & origin,
  ModuleProcessInformation * CLPProcessInformation
  )
{
  typedef TInputImage  InputImageType;
  typedef TOutputImage OutputImageType;

  typedef itk::SpecialCoordinatesImageToVTKStructuredGridFilter< InputImageType > ConversionFilterType;
  typename ConversionFilterType::Pointer conversionFilter = ConversionFilterType::New();
  conversionFilter->SetInput( inputImage );
  itk::PluginFilterWatcher watchConversion(conversionFilter, "Convert to vtkStructuredGrid", CLPProcessInformation);
  conversionFilter->Update();
  vtkStructuredGrid * inputStructuredGrid = conversionFilter->GetOutput();

  vtkNew< vtkImageData > grid;
  grid->SetDimensions( size[0], size[1], size[2] );
  grid->SetSpacing( spacing[0], spacing[1], spacing[2] );
  grid->SetOrigin( origin[0], origin[1], origin[2] );
  grid->ComputeBounds();

  vtkNew< vtkProbeFilter > probeFilter;
  probeFilter->SetSourceData( inputStructuredGrid );
  probeFilter->SetInputData( grid.GetPointer() );
  probeFilter->Update();

  typedef itk::VTKImageToImageFilter< OutputImageType > VTKToITKFilterType;
  typename VTKToITKFilterType::Pointer vtkToITKFilter = VTKToITKFilterType::New();
  vtkToITKFilter->SetInput( probeFilter->GetImageDataOutput() );
  vtkToITKFilter->Update();

  typename OutputImageType::Pointer output = OutputImageType::New();
  output->SetRegions( vtkToITKFilter->GetOutput()->GetLargestPossibleRegion() );
  output->Allocate();
  itk::ImageAlgorithm::Copy< OutputImageType, OutputImageType >(
    vtkToITKFilter->GetOutput(),
    output.GetPointer(),
    output->GetLargestPossibleRegion(),
    output->GetLargestPossibleRegion()
    );
  outputImage = output;


  return EXIT_SUCCESS;
}


template< typename TInputImage, typename TOutputImage >
int
VTKPointInterpolatorResampling(const typename TInputImage::Pointer & inputImage,
  typename TOutputImage::Pointer & outputImage,
  const typename TOutputImage::SizeType & size,
  const typename TOutputImage::SpacingType & spacing,
  const typename TOutputImage::PointType & origin,
  ScanConversionResamplingMethod method,
  ModuleProcessInformation * CLPProcessInformation
  )
{
  typedef TInputImage  InputImageType;
  typedef TOutputImage OutputImageType;

  typedef itk::SpecialCoordinatesImageToVTKStructuredGridFilter< InputImageType > ConversionFilterType;
  typename ConversionFilterType::Pointer conversionFilter = ConversionFilterType::New();
  conversionFilter->SetInput( inputImage );
  itk::PluginFilterWatcher watchConversion(conversionFilter, "Convert to vtkStructuredGrid", CLPProcessInformation);
  conversionFilter->Update();
  vtkStructuredGrid * inputStructuredGrid = conversionFilter->GetOutput();
  inputStructuredGrid->ComputeBounds();

  vtkNew< vtkImageData > grid;
  grid->SetDimensions( size[0], size[1], size[2] );
  grid->SetSpacing( spacing[0], spacing[1], spacing[2] );
  grid->SetOrigin( origin[0], origin[1], origin[2] );
  grid->ComputeBounds();
  vtkNew< vtkFloatArray > scalars;
  scalars->SetName( "Scalars" );
  scalars->Allocate( size[0] * size[1] * size[2] );
  grid->GetPointData()->SetScalars(scalars.GetPointer());


  vtkNew< vtkPointInterpolator > pointInterpolator;
  pointInterpolator->SetSourceData( inputStructuredGrid );
  pointInterpolator->SetInputData( grid.GetPointer() );
  pointInterpolator->SetPassPointArrays( false );
  pointInterpolator->SetNullPointsStrategyToNullValue();
  pointInterpolator->SetNullValue( 0.0 );

  vtkInterpolationKernel * interpolationKernel = ITK_NULLPTR;

  double maxSpacing = 0.0;
  for( unsigned int ii = 0; ii < InputImageType::ImageDimension; ++ii )
    {
    maxSpacing = std::max( maxSpacing, spacing[ii] );
    }
  const double radius = 1.1 * maxSpacing;

  switch( method )
    {
  case VTK_GAUSSIAN_KERNEL:
      {
      vtkGaussianKernel * gaussianKernel = vtkGaussianKernel::New();
      gaussianKernel->SetKernelFootprintToRadius();
      gaussianKernel->SetRadius( radius );
      interpolationKernel = gaussianKernel;
      break;
      }
  case VTK_LINEAR_KERNEL:
      {
      vtkLinearKernel * linearKernel = vtkLinearKernel::New();
      linearKernel->SetKernelFootprintToRadius();
      linearKernel->SetRadius( radius );
      interpolationKernel = linearKernel;
      break;
      }
  case VTK_SHEPARD_KERNEL:
      {
      vtkShepardKernel * shepardKernel = vtkShepardKernel::New();
      shepardKernel->SetKernelFootprintToRadius();
      shepardKernel->SetRadius( radius );
      interpolationKernel = shepardKernel;
      break;
      }
  case VTK_VORONOI_KERNEL:
      {
      vtkVoronoiKernel * voronoiKernel = vtkVoronoiKernel::New();
      interpolationKernel = voronoiKernel;
      break;
      }
  default:
    std::cerr << "Unexpected interpolation kernel: " << method << std::endl;
    return EXIT_FAILURE;
    }

  typedef itk::Image< float, 3 > VTKInterpolatorOutputImageType;
  typedef itk::VTKImageToImageFilter< VTKInterpolatorOutputImageType > VTKToITKFilterType;
  typename VTKToITKFilterType::Pointer vtkToITKFilter = VTKToITKFilterType::New();


  switch( method )
    {
  case VTK_GAUSSIAN_KERNEL:
  case VTK_LINEAR_KERNEL:
  case VTK_SHEPARD_KERNEL:
  case VTK_VORONOI_KERNEL:
      {
      pointInterpolator->SetKernel( interpolationKernel );
      pointInterpolator->Update();
      vtkToITKFilter->SetInput( pointInterpolator->GetImageDataOutput() );
      break;
      }
  default:
    std::cerr << "Unexpected interpolation kernel: " << method << std::endl;
    return EXIT_FAILURE;
    }

  vtkToITKFilter->Update();
  interpolationKernel->Delete();

  typedef itk::CastImageFilter< VTKInterpolatorOutputImageType, OutputImageType > CasterType;
  typename CasterType::Pointer caster = CasterType::New();
  caster->SetInput( vtkToITKFilter->GetOutput() );
  caster->Update();

  typename OutputImageType::Pointer output = OutputImageType::New();
  output->SetRegions( caster->GetOutput()->GetLargestPossibleRegion() );
  output->CopyInformation( caster->GetOutput() );
  output->Allocate();
  itk::ImageAlgorithm::Copy< OutputImageType, OutputImageType >(
    caster->GetOutput(),
    output.GetPointer(),
    output->GetLargestPossibleRegion(),
    output->GetLargestPossibleRegion()
    );
  outputImage = output;
  return EXIT_SUCCESS;
}


template< typename TInputImage, typename TOutputImage >
int
ScanConversionResampling(const typename TInputImage::Pointer & inputImage,
  typename TOutputImage::Pointer & outputImage,
  const typename TOutputImage::SizeType & size,
  const typename TOutputImage::SpacingType & spacing,
  const typename TOutputImage::PointType & origin,
  const typename TOutputImage::DirectionType & direction,
  const std::string & methodString,
  ModuleProcessInformation * CLPProcessInformation
  )
{
  typedef TInputImage  InputImageType;
  typedef TOutputImage OutputImageType;

  ScanConversionResamplingMethod method = ITK_LINEAR;
  if( methodString == "ITKNearestNeighbor" )
    {
    method = ITK_NEAREST_NEIGHBOR;
    }
  else if( methodString == "ITKLinear" )
    {
    method = ITK_LINEAR;
    }
  else if( methodString == "ITKWindowedSinc" )
    {
    method = ITK_WINDOWED_SINC;
    }
  else if( methodString == "VTKProbeFilter" )
    {
    method = VTK_PROBE_FILTER;
    }
  else if( methodString == "VTKGaussianKernel" )
    {
    method = VTK_GAUSSIAN_KERNEL;
    }
  else if( methodString == "VTKLinearKernel" )
    {
    method = VTK_LINEAR_KERNEL;
    }
  else if( methodString == "VTKShepardKernel" )
    {
    method = VTK_SHEPARD_KERNEL;
    }
  else if( methodString == "VTKVoronoiKernel" )
    {
    method = VTK_VORONOI_KERNEL;
    }

  switch( method )
    {
  case ITK_NEAREST_NEIGHBOR:
  case ITK_LINEAR:
  case ITK_WINDOWED_SINC:
    return ITKScanConversionResampling< InputImageType, OutputImageType >( inputImage,
      outputImage,
      size,
      spacing,
      origin,
      direction,
      method,
      CLPProcessInformation
    );
    break;
  case VTK_PROBE_FILTER:
    return VTKProbeFilterResampling< InputImageType, OutputImageType >( inputImage,
      outputImage,
      size,
      spacing,
      origin,
      CLPProcessInformation
    );
    break;
  case VTK_GAUSSIAN_KERNEL:
  case VTK_LINEAR_KERNEL:
  case VTK_SHEPARD_KERNEL:
  case VTK_VORONOI_KERNEL:
    return VTKPointInterpolatorResampling< InputImageType, OutputImageType >( inputImage,
      outputImage,
      size,
      spacing,
      origin,
      method,
      CLPProcessInformation
    );
    break;
  default:
    std::cerr << "Unknown scan conversion resampling method" << std::endl;
    }
  return EXIT_FAILURE;
}

}

#endif
